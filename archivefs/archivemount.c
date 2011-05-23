/*

   Copyright (c) 2005 Andre Landwehr <andrel@cybernoia.de>

   This program can be distributed under the terms of the GNU LGPL.
   See the file COPYING.

   Based on: fusexmp.c and sshfs.c by Miklos Szeredi <miklos@szeredi.hu>

*/

#include "archivemount.h"

/* For pthread_rwlock_t */
#define _GNU_SOURCE

#define MAXBUF 4096

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <utime.h>
#include <string.h>
#include <wchar.h>
#include <archive.h>
#include <archive_entry.h>
#include <pthread.h>

  /**********/
 /* macros */
/**********/
#ifdef NDEBUG
#   define log(format, ...)
#else
#   define log(format, ...) \
{ \
	FILE *FH = fopen( "/tmp/archivemount.log", "a" ); \
	if( FH ) { \
		fprintf( FH, "l. %4d: " format "\n", __LINE__, ##__VA_ARGS__ ); \
		fclose( FH ); \
	} \
}
#endif



enum
{
        KEY_VERSION,
        KEY_HELP,
};

#define AR_OPT(t, p, v) { t, offsetof(struct options, p), v }


  /**********************/
 /* internal functions */
/**********************/

static void
usage( const char *progname )
{
	fprintf(stderr,
			"usage: %s archivepath mountpoint [options]\n"
			"\n"
			"general options:\n"
			"    -o opt,[opt...]        mount options\n"
			"    -h   --help            print help\n"
			"    -V   --version         print version\n"
			"\n"
			"archivemount options:\n"
			"    -o readonly            disable write support\n"
			"    -o nobackup            remove archive file backups\n"
			"\n",progname);
}

//static struct fuse_operations ar_oper;

static void
init_node( NODE *node )
{
	node->parent = NULL;
	node->prev = NULL;
	node->next = NULL;
	node->child = NULL;
	node->name = NULL;
	node->location = NULL;
	node->namechanged = 0;
	node->entry = NULL;
	node->modified = 0;
}

static void
remove_child( NODE *node )
{
	if( node->prev ) {
		node->prev->next = node->next;
		//log( "removed '%s' from parent '%s' (prev was: '%s', next was '%s')", node->name, node->parent->name, node->prev->name, node->next?node->next->name:"NULL" );
	} else {
		if( node->parent ) {
			node->parent->child = node->next;
			//log( "removed '%s' from parent '%s' (was first child, next was '%s')", node->name, node->parent->name, node->next?node->next->name:"NULL" );
		}
	}
	if( node->next ) {
		node->next->prev = node->prev;
	}
}

static void
insert_as_child( NODE *node, NODE *parent )
{
	node->parent = parent;
	if( ! parent->child ) {
		parent->child = node;
		node->prev = NULL;
		node->next = NULL;
	} else {
		/* find last child of parent, insert node behind it */
		NODE *b = parent->child;
		while( b->next ) {
			b = b->next;
		}
		b->next = node;
		node->prev = b;
		node->next = NULL;
	}
	//log( "inserted '%s' as child of '%s', between '%s' and '%s'", node->name, parent->name, node->prev?node->prev->name:"NULL", node->next?node->next->name:"NULL" );
}

/*
 * inserts "node" into tree starting at "root" according to the path
 * specified in node->name
 * @return 0 on success, 0-errno else (ENOENT or ENOTDIR)
 */
static int
insert_by_path( NODE *root, NODE *node )
{
	char *temp;
	NODE *cur = root;
	char *key = node->name;

	key++;
	while( ( temp = strchr( key, '/' ) ) ) {
		size_t namlen = temp - key;
		char nam[namlen + 1];
		NODE *last = cur;

		strncpy( nam, key, namlen );
		nam[namlen] = '\0';
		if( strcmp( strrchr( cur->name, '/' ) + 1, nam ) != 0 ) {
			cur = cur->child;
			while( cur && strcmp( strrchr( cur->name, '/' ) + 1,
						nam ) != 0 )
			{
				cur = cur->next;
			}
		}
		if( ! cur ) {
			/* parent path not found, create a temporary one */
			NODE *tempnode;
			if( ( tempnode = malloc( sizeof( NODE ) ) ) == NULL ) {
			        log( "Out of memory" );
				return -ENOMEM;
			}
			init_node( tempnode );
			if( ( tempnode->name = malloc(
			        strlen( last->name ) + namlen + 1 ) ) == NULL ) {
			        log( "Out of memory" );
				return -ENOMEM;
			}
			if( last != root ) {
				sprintf( tempnode->name, "%s/%s", last->name, nam );
			} else {
				sprintf( tempnode->name, "/%s", nam );
			}
			if( (tempnode->entry = archive_entry_clone( root->entry )) == NULL ) {
			        log( "Out of memory" );
				return -ENOMEM;
			}
			/* insert it recursively */
			insert_by_path( root, tempnode );
			/* now inserting node should work, correct cur for it */
			cur = tempnode;
		}
		/* iterate */
		key = temp + 1;
	}
	if( S_ISDIR( archive_entry_mode( cur->entry ) ) || cur == root ) {
		/* check if a child of this name already exists */
		NODE *tempnode;
		int found = 0;
		tempnode = cur->child;
		while( tempnode ) {
			if( strcmp( strrchr( tempnode->name, '/' ) + 1,
						strrchr( node->name, '/' ) + 1 )
					== 0 )
			{
				/* this is a dupe due to a temporarily inserted
				   node, just update the entry */
				archive_entry_free( node->entry );
				if( (node->entry = archive_entry_clone(
						tempnode->entry )) == NULL) {
			                log( "Out of memory" );
				        return -ENOMEM;
				}
				found = 1;
				break;
			}
			tempnode = tempnode->next;
		}
		if( ! found ) {
			insert_as_child( node, cur );
		}
	} else {
		return -ENOTDIR;
	}
	return 0;
}

static int
build_tree( archive_fs_t *fs, const char *mtpt )
{
	struct archive *archive;
	struct archive_entry *entry;
	struct stat st;
	int format;
	int compression;

	/* open archive */
	if( (archive = archive_read_new()) == NULL ) {
	        log( "Out of memory" );
		return -ENOMEM;
	}
	if( archive_read_support_compression_all( archive ) != ARCHIVE_OK ) {
		fprintf( stderr, "%s\n", archive_error_string( archive ) );
		return archive_errno( archive );
	}
	if( archive_read_support_format_all( archive ) != ARCHIVE_OK ) {
		fprintf( stderr, "%s\n", archive_error_string( archive ) );
		return archive_errno( archive );
	}
	if( archive_read_open_fd( archive, fs->archiveFd, 10240 ) != ARCHIVE_OK ) {
		fprintf( stderr, "%s\n", archive_error_string( archive ) );
		return archive_errno( archive );
	}
	/* check if format or compression prohibits writability */
	format = archive_format( archive );
	compression = archive_compression( archive );
	if( format & ARCHIVE_FORMAT_ISO9660
			|| format & ARCHIVE_FORMAT_ISO9660_ROCKRIDGE
			|| format & ARCHIVE_FORMAT_ZIP
			|| compression == ARCHIVE_COMPRESSION_COMPRESS )
	{
		fs->archiveWriteable = 0;
	}
	/* create root node */
	if( (fs->root = malloc( sizeof( NODE ) ) ) == NULL ) {
	        log( "Out of memory" );
		return -ENOMEM;
	}
	init_node( fs->root );
	fs->root->name = strdup( "/" );
	/* fill root->entry */
	if( (fs->root->entry = archive_entry_new()) == NULL ) {
	        log( "Out of memory" );
		return -ENOMEM;
	}
	if( fstat( fs->archiveFd, &st ) != 0 ) {
		perror( "Error stat'ing archiveFile" );
		return errno;
	}
	archive_entry_set_gid( fs->root->entry, getgid() );
	archive_entry_set_uid( fs->root->entry, getuid() );
	archive_entry_set_mode( fs->root->entry, st.st_mtime );
	archive_entry_set_pathname( fs->root->entry, "/" );
	archive_entry_set_size( fs->root->entry, st.st_size );
	archive_entry_set_mode( fs->root->entry, 0777 );
	archive_entry_set_filetype( fs->root->entry, AE_IFDIR );
	/* read all entries in archive, create node for each */
	while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ) {
		NODE *cur;
		const char *name;
		/* find name of node */
		name = archive_entry_pathname( entry );
		if( strncmp( name, "./\0", 3 ) == 0 ) {
			/* special case: the directory "./" must be skipped! */
			continue;
		}
		/* create node and clone the entry */
		if( (cur = malloc( sizeof( NODE ) ) ) == NULL ) {
	                log( "Out of memory" );
		        return -ENOMEM;
		}
		init_node( cur );
		if( (cur->entry = archive_entry_clone( entry )) == NULL ) {
		        log( "Out of memory" );
			return -ENOMEM;
		}
		/* normalize the name to start with "/" */
		if( strncmp( name, "./", 2 ) == 0 ) {
			/* remove the "." of "./" */
			cur->name = strdup( name + 1 );
		} else if( name[0] != '/' ) {
			/* prepend a '/' to name */
		        if( (cur->name = malloc( strlen( name ) + 2 ) ) == NULL ) {
			        log( "Out of memory" );
				return -ENOMEM;
			}
			sprintf( cur->name, "/%s", name );
		} else {
			/* just set the name */
			cur->name = strdup( name );
		}
		/* remove trailing '/' for directories */
		if( cur->name[strlen(cur->name)-1] == '/' ) {
			cur->name[strlen(cur->name)-1] = '\0';
		}
		/* references */
		if( insert_by_path( fs->root, cur ) != 0 ) {
			log( "ERROR: could not insert %s into tree",
					cur->name );
			return -ENOENT;
		}
		archive_read_data_skip( archive );
	}
	/* close archive */
	archive_read_finish( archive );
	lseek( fs->archiveFd, 0, SEEK_SET );
	return 0;
}

static NODE *
find_modified_node( NODE *start )
{
	NODE *ret = NULL;
	NODE *run = start;

	while( run ) {
		if( run->modified ) {
			ret = run;
			break;
		}
		if( run->child ) {
			if( ( ret = find_modified_node( run->child ) ) ) {
				break;
			}
		}
		run = run->next;
	}
	return ret;
}

static void
correct_hardlinks_to_node( const NODE *start, const char *old_name,
		const char *new_name )
{
	const NODE *run = start;

	while( run ) {
		const char *tmp;
		if( ( tmp = archive_entry_hardlink( run->entry ) ) ) {
			if( strcmp( tmp, old_name ) == 0 ) {
				/* the node in "run" is a hardlink to "node",
				 * correct the path */
				//log( "correcting hardlink '%s' from '%s' to '%s'", run->name, old_name, new_name);
				archive_entry_set_hardlink(
						run->entry, new_name );
			}
		}
		if( run->child ) {
			correct_hardlinks_to_node( run->child, old_name, new_name );
		}
		run = run->next;
	}
}

static NODE *
get_node_for_path( NODE *start, const char *path )
{
	NODE *ret = NULL;
	NODE *run = start;

	while( run ) {
		if( strcmp( path, run->name ) == 0 ) {
			ret = run;
			break;
		}
		if( run->child && strncmp( path, run->name,
					strlen( run->name ) ) == 0 )
		{
			if( ( ret = get_node_for_path( run->child, path ) ) ) {
				break;
			}
		}
		run = run->next;
	}
	return ret;
}

static NODE *
get_node_for_entry( NODE *start, struct archive_entry *entry )
{
	NODE *ret = NULL;
	NODE *run = start;
	const char *path = archive_entry_pathname( entry );

	if( *path == '/' ) {
		path++;
	}
	while( run ) {
		const char *name = archive_entry_pathname( run->entry );
		if( *name == '/' ) {
			name++;
		}
		if( strcmp( path, name ) == 0 ) {
			ret = run;
			break;
		}
		if( run->child ) {
			if( ( ret = get_node_for_entry( run->child, entry ) ) )
			{
				break;
			}
		}
		run = run->next;
	}
	return ret;
}

static int
rename_recursively( archive_fs_t *fs, NODE *start, const char *from, const char *to )
{
	char *individualName;
	char *newName;
	int ret = 0;
	NODE *node = start;

	while( node ) {
		if( node->child ) {
			/* recurse */
			ret = rename_recursively( fs, node->child, from, to );
		}
		/* change node name */
		individualName = node->name + strlen( from );
		if( *to != '/' ) {
		        if( ( newName = ( char * )malloc( strlen( to ) +
			        strlen( individualName ) + 2 ) ) == NULL ) {
			        log( "Out of memory" );
				return -ENOMEM;
			}
			sprintf( newName, "/%s%s", to, individualName );
		} else {
		        if( ( newName = ( char * )malloc( strlen( to ) +
			        strlen( individualName ) + 1 ) ) == NULL ) {
			  log( "Out of memory" );
			  return -ENOMEM;
			}
			sprintf( newName, "%s%s", to, individualName );
		}
		correct_hardlinks_to_node( fs->root, node->name, newName );
		free( node->name );
		node->name = newName;
		node->namechanged = 1;
		/* iterate */
		node = node->next;
	}
	return ret;
}

static int
get_temp_file_name( const char *path, char **location )
{
	char *tmppath;
	char *tmp;
	int fh;

	/* create name for temp file */
	tmp = tmppath = strdup( path );
	do if( *tmp == '/' ) *tmp = '_'; while( *( tmp++ ) );
	if ( ( *location = ( char * )malloc(
			strlen( P_tmpdir ) +
			strlen( "_archivemount" ) +
			strlen( tmppath ) + 8 ) ) == NULL ) {
	        log( "Out of memory" );
		return -ENOMEM;
	}
	sprintf( *location, "%s/archivemount%s_XXXXXX", P_tmpdir, tmppath );
	free( tmppath );
	if( ( fh = mkstemp( *location ) == -1 ) ) {
		log( "Could not create temp file name %s: %s",
				*location, strerror( errno ) );
		free( *location );
		return 0 - errno;
	}
	close( fh );
	unlink( *location );
	return 0;
}

/**
 * Updates given nodes node->entry by stat'ing node->location. Does not update
 * the name!
 */
static int
update_entry_stat( NODE *node )
{
	struct stat st;
	struct passwd *pwd;
	struct group *grp;

	if( lstat( node->location, &st ) != 0 ) {
		return 0 - errno;
	}
	archive_entry_set_gid( node->entry, st.st_gid );
	archive_entry_set_uid( node->entry, st.st_uid );
	archive_entry_set_mtime( node->entry, st.st_mtime, 0 );
	archive_entry_set_size( node->entry, st.st_size );
	archive_entry_set_mode( node->entry, st.st_mode );
	archive_entry_set_rdevmajor( node->entry, st.st_dev );
	archive_entry_set_rdevminor( node->entry, st.st_dev );
	pwd = getpwuid( st.st_uid );
	if( pwd ) {
		archive_entry_set_uname( node->entry, strdup( pwd->pw_name ) );
	}
	grp = getgrgid( st.st_gid );
	if( grp ) {
		archive_entry_set_gname( node->entry, strdup( grp->gr_name ) );
	}
	return 0;
}

/*
 * write a new or modified file to the new archive; used from save()
 */
static void
write_new_modded_file( NODE *node, struct archive_entry *wentry,
		struct archive *newarc )
{
	if( node->location ) {
		struct stat st;
		int fh = 0;
		off_t offset = 0;
		void *buf;
		ssize_t len = 0;
		/* copy stat info */
		if( lstat( node->location, &st ) != 0 ) {
			log( "Could not lstat temporary file %s: %s",
					node->location,
					strerror( errno ) );
			return;
		}
		archive_entry_copy_stat( wentry, &st );
		/* open temporary file */
		fh = open( node->location, O_RDONLY );
		if( fh == -1 ) {
			log( "Fatal error opening modified file %s at "
					"location %s, giving up",
					node->name, node->location );
			return;
		}
		/* write header */
		archive_write_header( newarc, wentry );
		if( S_ISREG( st.st_mode ) ) {
			/* regular file, copy data */
		        if( ( buf = malloc( MAXBUF ) ) == NULL ) {
			        log( "Out of memory" );
				return;
			}
			while( ( len = pread( fh, buf, ( size_t )MAXBUF,
							offset ) ) > 0 )
			{
				archive_write_data( newarc, buf, len );
				offset += len;
			}
			free( buf );
		}
		if( len == -1 ) {
			log( "Error reading temporary file %s for file %s: %s",
					node->location,
					node->name,
					strerror( errno ) );
			close( fh );
			return;
		}
		/* clean up */
		close( fh );
		if( S_ISDIR( st.st_mode ) ) {
			if( rmdir( node->location ) == -1 ) {
				log( "WARNING: rmdir '%s' failed: %s",
						node->location, strerror( errno ) );
			}
		} else {
			if( unlink( node->location ) == -1 ) {
				log( "WARNING: unlinking '%s' failed: %s",
						node->location, strerror( errno ) );
			}
		}
	} else {
		/* no data, only write header (e.g. when node is a link!) */
		/* FIXME: hardlinks are saved, symlinks not. why??? */
		//log( "writing header for file %s", archive_entry_pathname( wentry ) );
		archive_write_header( newarc, wentry );
	}
	/* mark file as written */
	node->modified = 0;
}

static int
save( archive_fs_t *fs, const char *archiveFile )
{
	struct archive *oldarc;
	struct archive *newarc;
	struct archive_entry *entry;
	int tempfile;
	int format;
	int compression;
	char *oldfilename;
	NODE *node;

	oldfilename = malloc( strlen( archiveFile ) + 5 + 1 );
	if( !oldfilename ) {
		log( "Could not allocate memory for oldfilename" );
		return 0 - ENOMEM;
	}
	/* unfortunately libarchive does not support modification of
	 * compressed archives, so a new archive has to be written */
	/* rename old archive */
	sprintf( oldfilename, "%s.orig", archiveFile );
	close( fs->archiveFd );
	if( rename( archiveFile, oldfilename ) < 0 ) {
		int err = errno;
		char *buf = NULL;
		char *unknown = "<unknown>";
		if( getcwd( buf, 0 ) == NULL ) {
			/* getcwd failed, set buf to sth. reasonable */
			buf = unknown;
		}
		log( "Could not rename old archive file (%s/%s): %s",
				buf, archiveFile, strerror( err ) );
		free( buf );
		fs->archiveFd = open( fs->archiveFile, O_RDONLY );
		return 0 - err;
	}
	fs->archiveFd = open( oldfilename, O_RDONLY );
	free( oldfilename );
	/* open old archive */
	if( (oldarc = archive_read_new()) == NULL ) {
                log( "Out of memory" );
		return -ENOMEM;
	}
	if( archive_read_support_compression_all( oldarc ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( oldarc ) );
		return archive_errno( oldarc );
	}
	if( archive_read_support_format_all( oldarc ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( oldarc ) );
		return archive_errno( oldarc );
	}
	if( archive_read_open_fd( oldarc, fs->archiveFd, 10240 ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( oldarc ) );
		return archive_errno( oldarc );
	}
	format = archive_format( oldarc );
	compression = archive_compression( oldarc );
	/*
	log( "format of old archive is %s (%d)",
			archive_format_name( oldarc ),
			format );
	log( "compression of old archive is %s (%d)",
			archive_compression_name( oldarc ),
			compression );
	*/
	/* open new archive */
	if( (newarc = archive_write_new()) == NULL ) {
	        log( "Out of memory" );
		return -ENOMEM;
	}
	switch( compression ) {
		case ARCHIVE_COMPRESSION_GZIP:
			archive_write_set_compression_gzip( newarc );
			break;
		case ARCHIVE_COMPRESSION_BZIP2:
			archive_write_set_compression_bzip2( newarc );
			break;
		case ARCHIVE_COMPRESSION_COMPRESS:
		case ARCHIVE_COMPRESSION_NONE:
		default:
			archive_write_set_compression_none( newarc );
			break;
	}
#if 0
	if( archive_write_set_format( newarc, format ) != ARCHIVE_OK ) {
		return -ENOTSUP;
	}
#endif
	if( format & ARCHIVE_FORMAT_CPIO
			|| format & ARCHIVE_FORMAT_CPIO_POSIX )
	{
		archive_write_set_format_cpio( newarc );
		//log( "set write format to posix-cpio" );
	} else if( format & ARCHIVE_FORMAT_SHAR
			|| format & ARCHIVE_FORMAT_SHAR_BASE
			|| format & ARCHIVE_FORMAT_SHAR_DUMP )
	{
		archive_write_set_format_shar( newarc );
		//log( "set write format to binary shar" );
	} else if( format & ARCHIVE_FORMAT_TAR_PAX_RESTRICTED )
	{
		archive_write_set_format_pax_restricted( newarc );
		//log( "set write format to binary pax restricted" );
	} else if( format & ARCHIVE_FORMAT_TAR_PAX_INTERCHANGE )
	{
		archive_write_set_format_pax( newarc );
		//log( "set write format to binary pax interchange" );
	} else if( format & ARCHIVE_FORMAT_TAR_USTAR
			|| format & ARCHIVE_FORMAT_TAR
			|| format & ARCHIVE_FORMAT_TAR_GNUTAR
			|| format == 0 )
	{
		archive_write_set_format_ustar( newarc );
		//log( "set write format to ustar" );
	} else {
		log( "writing archives of format %d (%s) is not "
				"supported", format,
				archive_format_name( oldarc ) );
		return -ENOTSUP;
	}
	tempfile = open( archiveFile,
			O_WRONLY | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if( tempfile == -1 ) {
		log( "could not open new archive file for writing" );
		return 0 - errno;
	}
	if( archive_write_open_fd( newarc, tempfile ) != ARCHIVE_OK ) {
		log( "%s", archive_error_string( newarc ) );
		return archive_errno( newarc );
	}
	while( archive_read_next_header( oldarc, &entry ) == ARCHIVE_OK ) {
		off_t offset;
		const void *buf;
		struct archive_entry *wentry;
		size_t len;
		const char *name;
		/* find corresponding node */
		name = archive_entry_pathname( entry );
		node = get_node_for_entry( fs->root, entry );
		if( ! node ) {
			log( "WARNING: no such node for '%s'", name );
			archive_read_data_skip( oldarc );
			continue;
		}
		/* create new entry, copy metadata */
		if( (wentry = archive_entry_new()) == NULL ) {
		        log( "Out of memory" );
			return -ENOMEM;
		}
		if( archive_entry_gname_w( node->entry ) ) {
			archive_entry_copy_gname_w( wentry,
					archive_entry_gname_w( node->entry ) );
		}
		if( archive_entry_hardlink( node->entry ) ) {
			archive_entry_copy_hardlink( wentry,
					archive_entry_hardlink( node->entry ) );
		}
		if( archive_entry_hardlink_w( node->entry ) ) {
			archive_entry_copy_hardlink_w( wentry,
					archive_entry_hardlink_w(
						node->entry ) );
		}
		archive_entry_copy_stat( wentry,
				archive_entry_stat( node->entry ) );
		if( archive_entry_symlink_w( node->entry ) ) {
			archive_entry_copy_symlink_w( wentry,
					archive_entry_symlink_w(
						node->entry ) );
		}
		if( archive_entry_uname_w( node->entry ) ) {
			archive_entry_copy_uname_w( wentry,
					archive_entry_uname_w( node->entry ) );
		}
		/* set correct name */
		if( node->namechanged ) {
			if( *name == '/' ) {
				archive_entry_set_pathname(
						wentry, node->name );
			} else {
				archive_entry_set_pathname(
						wentry, node->name + 1 );
			}
		} else {
			archive_entry_set_pathname( wentry, name );
		}
		/* write header and copy data */
		if( node->modified ) {
			/* file was modified */
			write_new_modded_file( node, wentry, newarc );
		} else {
			/* file was not modified */
			archive_entry_copy_stat( wentry,
					archive_entry_stat( node->entry ) );
			archive_write_header( newarc, wentry );
			while( archive_read_data_block( oldarc, &buf,
						&len, &offset ) == ARCHIVE_OK )
			{
				archive_write_data( newarc, buf, len );
			}
		}
		/* clean up */
		archive_entry_free( wentry );
	} /* end: while read next header */
	/* find new files to add (those do still have modified flag set */
	while( ( node = find_modified_node( fs->root ) ) ) {
		write_new_modded_file( node, node->entry, newarc );
	}
	/* clean up, re-open the new archive for reading */
	archive_read_finish( oldarc );
	archive_write_finish( newarc );
	close( tempfile );
	close( fs->archiveFd );
	fs->archiveFd = open( fs->archiveFile, O_RDONLY );
	if( fs->options.nobackup ) {
		if( remove( oldfilename ) < 0 ) {
			log( "Could not remove .orig archive file (%s): %s",
					oldfilename, strerror( errno ) );
			return 0 - errno;
		}
	}
	return 0;
}


  /*****************/
 /* API functions */
/*****************/

int ar_init( archive_fs_t *fs, const char *archiveFile, const char *mtpt )
{
	fs->archiveModified = 0;
	fs->archiveWriteable = 0;
	fs->mtpt = strdup(mtpt);
	fs->archiveFile = strdup(archiveFile);
	fs->options.nobackup = 0;
	fs->options.readonly = 1;

	/* check if archive is writeable */
	fs->archiveFd = open( archiveFile, O_RDWR );
	if( fs->archiveFd != -1 ) {
		fs->archiveWriteable = 1;
		close( fs->archiveFd );
	}
	/* open archive and read meta data */
	fs->archiveFd = open( archiveFile, O_RDONLY );
	if( fs->archiveFd == -1 ) {
		perror( "opening archive failed" );
		return EXIT_FAILURE;
	}
	build_tree( fs, fs->mtpt );
	
	/* Initialize the node tree lock */
	pthread_rwlock_init(&fs->lock, NULL);

	return EXIT_SUCCESS;
}

int ar_free( archive_fs_t *fs )
{
#if 0
	/* save changes if modified */
	if( archiveWriteable && !options.readonly && archiveModified ) {
		if( save( fs->archiveFile ) != 0 ) {
			fprintf( stderr, "Saving new archive failed\n" );
		}
	}
#endif
	
	/* clean up */
	close( fs->archiveFd );
	
	free( fs->archiveFile );
	free( fs->mtpt );
	
	return 0;
}

static int
_ar_read( archive_fs_t *fs, const char *path, char *buf, size_t size, off_t offset )
{
	int ret = -1;
	const char *realpath;
	NODE *node;

	//log( "read called, path: '%s'", path );
	/* find node */
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return _ar_read( fs, archive_entry_hardlink( node->entry ),
				buf, size, offset );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return _ar_read( fs, archive_entry_symlink( node->entry ),
				buf, size, offset );
	}
	if( node->modified ) {
		/* the file is new or modified, read temporary file instead */
		int fh;
		fh = open( node->location, O_RDONLY );
		if( fh == -1 ) {
			log( "Fatal error opening modified file '%s' at "
					"location '%s', giving up",
					path, node->location );
			return 0 - errno;
		}
		/* copy data */
		if( ( ret = pread( fh, buf, size, offset ) ) == -1 ) {
			log( "Error reading temporary file '%s': %s",
					node->location, strerror( errno ) );
			close( fh );
			ret = 0 - errno;
		}
		/* clean up */
		close( fh );
	} else {
		struct archive *archive;
		struct archive_entry *entry;
		/* search file in archive */
		realpath = archive_entry_pathname( node->entry );
		if( (archive = archive_read_new()) == NULL ) {
		        log( "Out of memory" );
			return -ENOMEM;
		}
		if( archive_read_support_compression_all( archive ) != ARCHIVE_OK ) {
			log( "%s", archive_error_string( archive ) );
		}
		if( archive_read_support_format_all( archive ) != ARCHIVE_OK ) {
			log( "%s", archive_error_string( archive ) );
		}
		if( archive_read_open_fd( archive, fs->archiveFd, 10240 ) != ARCHIVE_OK ) {
			log( "%s", archive_error_string( archive ) );
		}
		/* search for file to read */
		while( archive_read_next_header( archive, &entry ) == ARCHIVE_OK ) {
			const char *name;
			name = archive_entry_pathname( entry );
			if( strcmp( realpath, name ) == 0 ) {
				void *trash;
				if( ( trash = malloc( MAXBUF ) ) == NULL ) {
				        log( "Out of memory" );
					return -ENOMEM;
				}
				/* skip offset */
				while( offset > 0 ) {
					int skip = offset > MAXBUF ? MAXBUF : offset;
					ret = archive_read_data(
							archive, trash, skip );
					if( ret == ARCHIVE_FATAL
							|| ret == ARCHIVE_WARN
							|| ret == ARCHIVE_RETRY )
					{
						log( "ar_read (skipping offset): %s",
							archive_error_string( archive ) );
						errno = archive_errno( archive );
						ret = -1;
						break;
					}
					offset -= skip;
				}
				free( trash );
				if( offset ) {
					/* there was an error */
					break;
				}
				/* read data */
				ret = archive_read_data( archive, buf, size );
				if( ret == ARCHIVE_FATAL
						|| ret == ARCHIVE_WARN
						|| ret == ARCHIVE_RETRY )
				{
					log( "ar_read (reading data): %s",
						archive_error_string( archive ) );
					errno = archive_errno( archive );
					ret = -1;
				}
				break;
			}
			archive_read_data_skip( archive );
		}
		/* close archive */
		archive_read_finish( archive );
		lseek( fs->archiveFd, 0, SEEK_SET );
	}
	return ret;
}

int
ar_read( archive_fs_t *fs, const char *path, char *buf, size_t size, off_t offset )
{
	int ret;
	pthread_rwlock_rdlock(&fs->lock);
	ret = _ar_read( fs, path, buf, size, offset );
	pthread_rwlock_unlock(&fs->lock);
	return ret;
}

static int
_ar_getattr( archive_fs_t* fs, const char *path, struct stat *stbuf )
{
	NODE *node;
	int ret;

	//log( "getattr called, path: '%s'", path );
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* a hardlink, recurse into it */
		ret = _ar_getattr( fs, archive_entry_hardlink(
					node->entry ), stbuf );
		return ret;
	}
	memcpy( stbuf, archive_entry_stat( node->entry ),
			sizeof( struct stat ) );

	if( fs->options.readonly ) {
		stbuf->st_mode = stbuf->st_mode & 0777555;
	}

	return 0;
}

int
ar_getattr( archive_fs_t* fs, const char *path, struct stat *stbuf )
{
	int ret;
	pthread_rwlock_rdlock(&fs->lock);
	ret = _ar_getattr( fs, path, stbuf );
	pthread_rwlock_unlock(&fs->lock);
	return ret;
}

/*
 * mkdir is nearly identical to mknod...
 */
int ar_mkdir( archive_fs_t* fs, const char *path, mode_t mode )
{
	NODE *node;
	char *location;
	int tmp;

	//log( "mkdir called, path '%s', mode %o", path, mode );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	/* check for existing node */
	node = get_node_for_path( fs->root, path );
	if( node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EEXIST;
	}
	/* create name for temp dir */
	if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
		pthread_rwlock_unlock( &fs->lock );
		return tmp;
	}
	/* create temp dir */
	if( mkdir( location, mode ) == -1 ) {
		log( "Could not create temporary dir %s: %s",
				location, strerror( errno ) );
		free( location );
		pthread_rwlock_unlock( &fs->lock );
		return 0 - errno;
	}
	/* build node */
	if( ( node = ( NODE * )malloc( sizeof( NODE ) ) ) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	init_node( node );
	node->location = location;
	node->modified = 1;
	node->name = strdup( path );
	node->namechanged = 0;
	/* build entry */
	if( (node->entry = archive_entry_new()) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	if( fs->root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( fs->root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "mkdir: error stat'ing dir %s: %s", node->location,
				strerror( 0 - tmp ) );
		rmdir( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return tmp;
	}
	/* add node to tree */
	if( insert_by_path( fs->root, node ) != 0 ) {
		log( "ERROR: could not insert %s into tree",
				node->name );
		rmdir( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	/* clean up */
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

/*
 * ar_rmdir is called for directories only and does not need to do any
 * recursive stuff
 */
int ar_rmdir( archive_fs_t* fs, const char *path )
{
	NODE *node;

	//log( "rmdir called, path '%s'", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	if( node->child ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOTEMPTY;
	}
	if( node->name[strlen(node->name)-1] == '.' ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EINVAL;
	}
	if( ! S_ISDIR( archive_entry_mode( node->entry ) ) ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOTDIR;
	}
	if( node->location ) {
		/* remove temp directory */
		if( rmdir( node->location ) == -1 ) {
			int err = errno;
			log( "ERROR: removing temp directory %s failed: %s",
					node->location, strerror( err ) );
			pthread_rwlock_unlock( &fs->lock );
			return err;
		}
		free( node->location );
	}
	remove_child( node );
	free( node->name );
	free( node );
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

int ar_symlink( archive_fs_t* fs, const char *from, const char *to )
{
	NODE *node;
	struct stat st;
	struct passwd *pwd;
	struct group *grp;

	return -ENOSYS; /* somehow saving symlinks does not work. The code below is ok.. see write_new_modded_file() */
	//log( "symlink called, %s -> %s", from, to );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	/* check for existing node */
	node = get_node_for_path( fs->root, to );
	if( node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EEXIST;
	}
	/* build node */
	if( ( node = ( NODE * )malloc( sizeof( NODE ) ) ) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	init_node( node );
	node->name = strdup( to );
	node->modified = 1;
	/* build stat info */
	st.st_dev = 0;
	st.st_ino = 0;
	st.st_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
	st.st_nlink = 1;
	st.st_uid = getuid();
	st.st_gid = getgid();
	st.st_rdev = 0;
	st.st_size = strlen( from );
	st.st_blksize = 4096;
	st.st_blocks = 0;
	st.st_atime = st.st_ctime = st.st_mtime = time( NULL );
	/* build entry */
	if( (node->entry = archive_entry_new()) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	if( fs->root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( fs->root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	archive_entry_copy_stat( node->entry, &st );
	archive_entry_set_symlink( node->entry, strdup( from ) );
	/* get user/group name */
	pwd = getpwuid( st.st_uid );
	if( pwd ) {
		/* a name was found for the uid */
		archive_entry_set_uname( node->entry, strdup( pwd->pw_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getpwuid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			pthread_rwlock_unlock( &fs->lock );
			return 0 - errno;
		}
		/* on other errors the uid just could
		   not be resolved into a name */
	}
	grp = getgrgid( st.st_gid );
	if( grp ) {
		/* a name was found for the uid */
		archive_entry_set_gname( node->entry, strdup( grp->gr_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getgrgid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			pthread_rwlock_unlock( &fs->lock );
			return 0 - errno;
		}
		/* on other errors the gid just could
		   not be resolved into a name */
	}
	/* add node to tree */
	if( insert_by_path( fs->root, node ) != 0 ) {
		log( "ERROR: could not insert symlink %s into tree",
				node->name );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	/* clean up */
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

int ar_link( archive_fs_t* fs, const char *from, const char *to )
{
	NODE *node;
	NODE *fromnode;
	struct stat st;
	struct passwd *pwd;
	struct group *grp;

	//log( "hardlink called, %s -> %s", from, to );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	/* find source node */
	fromnode = get_node_for_path( fs->root, from );
	if( ! fromnode ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	/* check for existing target */
	node = get_node_for_path( fs->root, to );
	if( node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EEXIST;
	}
	/* extract originals stat info */
	_ar_getattr( fs, from, &st );
	/* build new node */
	if( (node = ( NODE * )malloc( sizeof( NODE ) ) ) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	init_node( node );
	node->name = strdup( to );
	node->modified = 1;
	/* build entry */
	if( (node->entry = archive_entry_new()) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	if( node->name[0] == '/' &&
			archive_entry_pathname( fromnode->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	archive_entry_copy_stat( node->entry, &st );
	archive_entry_set_hardlink( node->entry, strdup( from ) );
	/* get user/group name */
	pwd = getpwuid( st.st_uid );
	if( pwd ) {
		/* a name was found for the uid */
		archive_entry_set_uname( node->entry, strdup( pwd->pw_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getpwuid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			pthread_rwlock_unlock( &fs->lock );
			return 0 - errno;
		}
		/* on other errors the uid just could
		   not be resolved into a name */
	}
	grp = getgrgid( st.st_gid );
	if( grp ) {
		/* a name was found for the uid */
		archive_entry_set_gname( node->entry, strdup( grp->gr_name ) );
	} else {
		if( errno == EINTR || errno == EIO || errno == EMFILE || 
				errno == ENFILE || errno == ENOMEM ||
				errno == ERANGE )
		{
			log( "ERROR calling getgrgid: %s", strerror( errno ) );
			free( node->name );
			archive_entry_free( node->entry );
			free( node );
			pthread_rwlock_unlock( &fs->lock );
			return 0 - errno;
		}
		/* on other errors the gid just could
		   not be resolved into a name */
	}
	/* add node to tree */
	if( insert_by_path( fs->root, node ) != 0 ) {
		log( "ERROR: could not insert hardlink %s into tree",
				node->name );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	/* clean up */
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

static int
_ar_truncate( archive_fs_t* fs, const char *path, off_t size )
{
	NODE *node;
	char *location;
	int ret;
	int tmp;
	int fh;

	//log( "truncate called, path '%s'", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return _ar_truncate( fs, archive_entry_hardlink(
					node->entry ), size );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return _ar_truncate( fs, archive_entry_symlink(
					node->entry ), size );
	}
	if( node->location ) {
		/* open existing temp file */
		location = node->location;
		if( ( fh = open( location, O_WRONLY ) ) == -1 ) {
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
	} else {
		/* create new temp file */
		char *tmpbuf = NULL;
		int tmpoffset = 0;
		int64_t tmpsize;
		if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
			return tmp;
		}
		if( ( fh = open( location, O_WRONLY | O_CREAT | O_EXCL,
				archive_entry_mode( node->entry ) ) ) == -1 )
		{
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
		/* copy original file to temporary file */
		tmpsize = archive_entry_size( node->entry );
		if( ( tmpbuf = ( char * )malloc( MAXBUF ) ) == NULL ) {
	                log( "Out of memory" );
		        return -ENOMEM;
		}
		while( tmpsize ) {
			int len = tmpsize > MAXBUF ? MAXBUF : tmpsize;
			/* read */
			if( ( tmp = _ar_read( fs, path, tmpbuf, len, tmpoffset ) )
					< 0 )
			{
				log( "ERROR reading while copying %s to "
						"temporary location %s: %s",
						path, location,
						strerror( 0 - tmp ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			/* write */
			if( write( fh, tmpbuf, tmp ) == -1 ) {
				tmp = 0 - errno;
				log( "ERROR writing while copying %s to "
					       "temporary location %s: %s",
						path, location,
						strerror( errno ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			tmpsize -= len;
			tmpoffset += len;
			if( tmpoffset >= size ) {
				/* copied enough, exit the loop */
				break;
			}
		}
		/* clean up */
		free( tmpbuf );
		lseek( fh, 0, SEEK_SET );
	}
	/* truncate temporary file */
	if( ( ret = truncate( location, size ) ) == -1 ) {
		tmp = 0 - errno;
		log( "ERROR truncating %s (temporary location %s): %s",
				path, location, strerror( errno ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* record location, update entry */
	node->location = location;
	node->modified = 1;
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "write: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* clean up */
	close( fh );
	fs->archiveModified = 1;
	return ret;
}

int ar_truncate( archive_fs_t* fs, const char *path, off_t size )
{
	int ret;
	pthread_rwlock_wrlock( &fs->lock );
	ret = _ar_truncate( fs, path, size );
	pthread_rwlock_unlock( &fs->lock );
	return ret;
}

static int
_ar_write( archive_fs_t* fs, const char *path, const char *buf, size_t size,
		off_t offset )
{
	NODE *node;
	char *location;
	int ret;
	int tmp;
	int fh;

	//log( "write called, path '%s'", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( S_ISLNK( archive_entry_mode( node->entry ) ) ) {
		/* file is a symlink, recurse into it */
		return _ar_write( fs, archive_entry_symlink( node->entry ),
				buf, size, offset );
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return _ar_write( fs, archive_entry_hardlink( node->entry ),
				buf, size, offset );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return _ar_write( fs, archive_entry_symlink( node->entry ),
				buf, size, offset );
	}
	if( node->location ) {
		/* open existing temp file */
		location = node->location;
		if( ( fh = open( location, O_WRONLY ) ) == -1 ) {
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
	} else {
		/* create new temp file */
		char *tmpbuf = NULL;
		int tmpoffset = 0;
		int64_t tmpsize;
		if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
			return tmp;
		}
		if( ( fh = open( location, O_WRONLY | O_CREAT | O_EXCL,
				archive_entry_mode( node->entry ) ) ) == -1 )
		{
			log( "error opening temp file %s: %s",
					location, strerror( errno ) );
			unlink( location );
			return 0 - errno;
		}
		/* copy original file to temporary file */
		tmpsize = archive_entry_size( node->entry );
		if( ( tmpbuf = ( char * )malloc( MAXBUF ) ) == NULL ) {
	                log( "Out of memory" );
		        return -ENOMEM;
		}
		while( tmpsize ) {
			int len = tmpsize > MAXBUF ? MAXBUF : tmpsize;
			/* read */
			if( ( tmp = _ar_read( fs, path, tmpbuf, len, tmpoffset ) )
					< 0 )
			{
				log( "ERROR reading while copying %s to "
						"temporary location %s: %s",
						path, location,
						strerror( 0 - tmp ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			/* write */
			if( write( fh, tmpbuf, len ) == -1 ) {
				tmp = 0 - errno;
				log( "ERROR writing while copying %s to "
					       "temporary location %s: %s",
						path, location,
						strerror( errno ) );
				close( fh );
				unlink( location );
				free( tmpbuf );
				return tmp;
			}
			tmpsize -= len;
			tmpoffset += len;
		}
		/* clean up */
		free( tmpbuf );
		lseek( fh, 0, SEEK_SET );
	}
	/* write changes to temporary file */
	if( ( ret = pwrite( fh, buf, size, offset ) ) == -1 ) {
		tmp = 0 - errno;
		log( "ERROR writing changes to %s (temporary "
				"location %s): %s",
				path, location, strerror( errno ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* record location, update entry */
	node->location = location;
	node->modified = 1;
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "write: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		close( fh );
		unlink( location );
		return tmp;
	}
	/* clean up */
	close( fh );
	fs->archiveModified = 1;
	return ret;
}

int ar_write( archive_fs_t* fs, const char *path, const char *buf, size_t size,
		off_t offset )
{
	int ret;
	pthread_rwlock_wrlock(&fs->lock);
	ret = _ar_write( fs, path, buf, size, offset );
	pthread_rwlock_unlock(&fs->lock);
	return ret;
}

int
ar_mknod( archive_fs_t* fs, const char *path, mode_t mode, dev_t rdev )
{
	NODE *node;
	char *location;
	int tmp;

	//log( "mknod called, path %s", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	/* check for existing node */
	node = get_node_for_path( fs->root, path );
	if( node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EEXIST;
	}
	/* create name for temp file */
	if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
		pthread_rwlock_unlock( &fs->lock );
		return tmp;
	}
	/* create temp file */
	if( mknod( location, mode, rdev ) == -1 ) {
		log( "Could not create temporary file %s: %s",
				location, strerror( errno ) );
		free( location );
		pthread_rwlock_unlock( &fs->lock );
		return 0 - errno;
	}
	/* build node */
	if( ( node = ( NODE * )malloc( sizeof( NODE ) ) ) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	init_node( node );
	node->location = location;
	node->modified = 1;
	node->name = strdup( path );
	/* build entry */
	if( (node->entry = archive_entry_new()) == NULL) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	if( fs->root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( fs->root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "mknod: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		unlink( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return tmp;
	}
	/* add node to tree */
	if( insert_by_path( fs->root, node ) != 0 ) {
		log( "ERROR: could not insert %s into tree",
				node->name );
		unlink( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	/* clean up */
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

int ar_unlink( archive_fs_t* fs, const char *path )
{
	NODE *node;

	//log( "unlink called, %s", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	if( S_ISDIR( archive_entry_mode( node->entry ) ) ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EISDIR;
	}
	if( node->location ) {
		/* remove temporary file */
		if( unlink( node->location ) == -1 ) {
			int err = errno;
			log( "ERROR: could not unlink temporary file '%s': %s",
					node->location, strerror( errno ) );
			pthread_rwlock_unlock( &fs->lock );
			return err;
		}
		free( node->location );
	}
	remove_child( node );
	free( node->name );
	free( node );
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

static int
_ar_chmod( archive_fs_t* fs, const char *path, mode_t mode )
{
	NODE *node;

	//log( "chmod called, path '%s', mode: %o", path, mode );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return _ar_chmod( fs, archive_entry_hardlink( node->entry ), mode );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return _ar_chmod( fs, archive_entry_symlink( node->entry ), mode );
	}
#ifdef __APPLE__
	/* Make sure the full mode, including file type information, is used */
	mode = (0777000 & archive_entry_mode(node->entry)) | (0000777 & mode);
#endif // __APPLE__
	archive_entry_set_mode( node->entry, mode );
	fs->archiveModified = 1;
	return 0;
}

int ar_chmod( archive_fs_t* fs, const char *path, mode_t mode )
{
	int ret;
	pthread_rwlock_wrlock( &fs->lock );
	ret = _ar_chmod( fs, path, mode );
	pthread_rwlock_unlock( &fs->lock );
	return ret;
}

static int
_ar_chown( archive_fs_t* fs, const char *path, uid_t uid, gid_t gid )
{
	NODE *node;

	//log( "chown called, %s", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return _ar_chown( fs, archive_entry_hardlink( node->entry ),
				uid, gid );
	}
	/* changing ownership of symlinks is allowed, however */
	archive_entry_set_uid( node->entry, uid );
	archive_entry_set_gid( node->entry, gid );
	fs->archiveModified = 1;
	return 0;
}

int ar_chown( archive_fs_t* fs, const char *path, uid_t uid, gid_t gid )
{
	int ret;
	pthread_rwlock_wrlock( &fs->lock );
	ret = _ar_chown( fs, path, uid, gid );
	pthread_rwlock_unlock( &fs->lock );
	return ret;
}

static int
_ar_utime( archive_fs_t* fs, const char *path, struct utimbuf *buf )
{
	NODE *node;

	//log( "utime called, %s", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		return -ENOENT;
	}
	if( archive_entry_hardlink( node->entry ) ) {
		/* file is a hardlink, recurse into it */
		return _ar_utime( fs, archive_entry_hardlink( node->entry ), buf );
	}
	if( archive_entry_symlink( node->entry ) ) {
		/* file is a symlink, recurse into it */
		return _ar_utime( fs, archive_entry_symlink( node->entry ), buf );
	}
	archive_entry_set_mtime( node->entry, buf->modtime, 0 );
	archive_entry_set_atime( node->entry, buf->actime, 0 );
	fs->archiveModified = 1;
	return 0;
}

int ar_utime( archive_fs_t* fs, const char *path, struct utimbuf *buf )
{
	int ret;
	pthread_rwlock_wrlock( &fs->lock );
	ret = _ar_utime( fs, path, buf );
	pthread_rwlock_unlock( &fs->lock );
	return ret;
}

int ar_statfs( archive_fs_t* fs, const char *path, struct statvfs *stbuf )
{
  	( void )path;

	//log( "statfs called, %s", path );

	/* Adapted the following from sshfs.c */

	stbuf->f_namemax = 255;
	stbuf->f_bsize = 4096;
	/*
	 * df seems to use f_bsize instead of f_frsize, so make them
	 * the same
	 */
	stbuf->f_frsize = stbuf->f_bsize;
	stbuf->f_blocks = stbuf->f_bfree =  stbuf->f_bavail =
		1000ULL * 1024 * 1024 * 1024 / stbuf->f_frsize;
	stbuf->f_files = stbuf->f_ffree = 1000000000;
	return 0;
}

int ar_rename( archive_fs_t* fs, const char *from, const char *to )
{
	NODE *node;
	int ret = 0;
	char *old_name;

	//log( "ar_rename called, from: '%s', to: '%s'", from, to );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock(&fs->lock);
	node = get_node_for_path( fs->root, from );
	if( ! node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	if( node->child ) {
		/* it is a directory, recursive change of all nodes
		 * below it is required */
		ret = rename_recursively( fs, node->child, from, to );
	}
	/* meta data is changed in save() */
	/* change node name */
	if( *to != '/' ) {
		char *temp_name;
	        if( ( temp_name = malloc( strlen( to ) + 2 ) ) == NULL ) {
	                log( "Out of memory" );
			pthread_rwlock_unlock( &fs->lock );
		        return -ENOMEM;
		}
		sprintf( temp_name, "/%s", to );
		old_name = node->name;
		node->name = temp_name;
	} else {
		char *temp_name;
		if( ( temp_name = strdup( to ) ) == NULL ) {
	                log( "Out of memory" );
			pthread_rwlock_unlock( &fs->lock );
		        return -ENOMEM;
		}
		old_name = node->name;
		node->name = temp_name;
	}
	node->namechanged = 1;
	remove_child( node );
	ret = insert_by_path( fs->root, node );
	correct_hardlinks_to_node( fs->root, old_name, node->name );
	free( old_name );
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return ret;
}

int ar_fsync( archive_fs_t* fs, const char *path, int isdatasync )
{
	/* Just a stub.  This method is optional and can safely be left
	   unimplemented */
	( void )path;
	( void )isdatasync;
	return 0;
}

int ar_readlink( archive_fs_t* fs, const char *path, char *buf, size_t size )
{
	NODE *node;
	const char *tmp;

	//log( "readlink called, path '%s'", path );
	pthread_rwlock_rdlock( &fs->lock );
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	if( ! S_ISLNK( archive_entry_mode( node->entry ) ) ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOLINK;
	}
	tmp = archive_entry_symlink( node->entry );
	snprintf( buf, size, "%s", tmp );
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

int ar_open( archive_fs_t* fs, const char *path, int flags )
{
	NODE *node;

	//log( "open called, path '%s'", path );
	pthread_rwlock_rdlock( &fs->lock );
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	if( flags & O_WRONLY || flags & O_RDWR ) {
		if( ! fs->archiveWriteable ) {
			pthread_rwlock_unlock( &fs->lock );
			return -EROFS;
		}
	}
	/* no need to recurse into links since function doesn't do anything */
	/* no need to save a handle here since archives are stream based */
	//fi->fh = 0;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

int ar_release( archive_fs_t* fs, const char *path )
{
	( void )fs;
	( void )path;
	return 0;
}

int ar_readdir( archive_fs_t* fs, const char *path, void *buf, fill_dir_t filler,
		off_t offset )
{
	NODE *node;
	(void) offset;

	//log( "readdir called, path: '%s'", path );
	pthread_rwlock_rdlock( &fs->lock );
	node = get_node_for_path( fs->root, path );
	if( ! node ) {
		log( "path '%s' not found", path );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}

        filler( ".", NULL );
        filler( "..", NULL );

	node = node->child;
	while( node ) {
		struct stat st;
		char *name;
		st.st_ino = archive_entry_ino( node->entry );
		st.st_mode = archive_entry_mode( node->entry );
		name = strrchr( node->name, '/' ) + 1;
		if( filler( name, &st ) )
			break;
		node = node->next;
	}
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

int ar_create( archive_fs_t* fs, const char *path, mode_t mode )
{
	NODE *node;
	char *location;
	int tmp;

	/* the implementation of this function is mostly copy-paste from
	   mknod, with the exception that the temp file is created with
	   creat() instead of mknod() */
	//log( "create called, path '%s'", path );
	if( ! fs->archiveWriteable || fs->options.readonly ) {
		return -EROFS;
	}
	pthread_rwlock_wrlock( &fs->lock );
	/* check for existing node */
	node = get_node_for_path( fs->root, path );
	if( node ) {
		pthread_rwlock_unlock( &fs->lock );
		return -EEXIST;
	}
	/* create name for temp file */
	if( ( tmp = get_temp_file_name( path, &location ) < 0 ) ) {
		pthread_rwlock_unlock( &fs->lock );
		return tmp;
	}
	/* create temp file */
	if( creat( location, mode ) == -1 ) {
		log( "Could not create temporary file %s: %s",
				location, strerror( errno ) );
		free( location );
		pthread_rwlock_unlock( &fs->lock );
		return 0 - errno;
	}
	/* build node */
	if( ( node = ( NODE * )malloc( sizeof( NODE ) ) ) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	init_node( node );
	node->location = location;
	node->modified = 1;
	node->name = strdup( path );
	/* build entry */
	if( (node->entry = archive_entry_new()) == NULL ) {
	        log( "Out of memory" );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOMEM;
	}
	if( fs->root->child &&
			node->name[0] == '/' &&
			archive_entry_pathname( fs->root->child->entry )[0] != '/' )
	{
		archive_entry_set_pathname( node->entry, node->name + 1 );
	} else {
		archive_entry_set_pathname( node->entry, node->name );
	}
	if( ( tmp = update_entry_stat( node ) ) < 0 ) {
		log( "mknod: error stat'ing file %s: %s", node->location,
				strerror( 0 - tmp ) );
		unlink( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return tmp;
	}
	/* add node to tree */
	if( insert_by_path( fs->root, node ) != 0 ) {
		log( "ERROR: could not insert %s into tree",
				node->name );
		unlink( location );
		free( location );
		free( node->name );
		archive_entry_free( node->entry );
		free( node );
		pthread_rwlock_unlock( &fs->lock );
		return -ENOENT;
	}
	/* clean up */
	fs->archiveModified = 1;
	pthread_rwlock_unlock( &fs->lock );
	return 0;
}

#if 0
static struct fuse_operations ar_oper = {
	.getattr	= ar_getattr,
	.readlink	= ar_readlink,
	.mknod		= ar_mknod,
	.mkdir		= ar_mkdir,
	.symlink	= ar_symlink,
	.unlink		= ar_unlink,
	.rmdir		= ar_rmdir,
	.rename		= ar_rename,
	.link		= ar_link,
	.chmod		= ar_chmod,
	.chown		= ar_chown,
	.truncate	= ar_truncate,
	.utime		= ar_utime,
	.open		= ar_open,
	.read		= ar_read,
	.write		= ar_write,
	.statfs		= ar_statfs,
	//.flush          = ar_flush,  // int(*flush )(const char *, struct fuse_file_info *)
	.release	= ar_release,
	.fsync		= ar_fsync,
/*
#ifdef HAVE_SETXATTR
	.setxattr	= ar_setxattr,
	.getxattr	= ar_getxattr,
	.listxattr	= ar_listxattr,
	.removexattr	= ar_removexattr,
#endif
*/
	//.opendir        = ar_opendir,    // int(*opendir )(const char *, struct fuse_file_info *)
	.readdir	= ar_readdir,
	//.releasedir     = ar_releasedir, // int(*releasedir )(const char *, struct fuse_file_info *)
	//.fsyncdir       = ar_fsyncdir,   // int(*fsyncdir )(const char *, int, struct fuse_file_info *)
	//.init           = ar_init,       // void *(*init )(struct fuse_conn_info *conn)
	//.destroy        = ar_destroy,    // void(*destroy )(void *)
	//.access         = ar_access,     // int(*access )(const char *, int)
	.create         = ar_create,
	//.ftruncate      = ar_ftruncate,  // int(*ftruncate )(const char *, off_t, struct fuse_file_info *)
	//.fgetattr       = ar_fgetattr,   // int(*fgetattr )(const char *, struct stat *, struct fuse_file_info *)
	//.lock           = ar_lock,       // int(*lock )(const char *, struct fuse_file_info *, int cmd, struct flock *)
	//.utimens        = ar_utimens,    // int(*utimens )(const char *, const struct timespec tv[2])
	//.bmap           = ar_bmap,       // int(*bmap )(const char *, size_t blocksize, uint64_t *idx)
};
#endif

void
showUsage()
{
	fprintf( stderr, "Usage: archivemount <fuse-options> <archive> <mountpoint>\n" );
	fprintf( stderr, "Usage:              (-v|--version)\n" );
}

#if 0

int
main( int argc, char **argv )
{
	int fuse_ret;
	struct stat st;
	int oldpwd;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/* parse cmdline args */
	memset( &options, 0, sizeof( struct options ) );
	if( fuse_opt_parse( &args, &options, ar_opts, ar_opt_proc ) == -1 )
                return -1;
	if( archiveFile==NULL ) {
                fprintf(stderr, "missing archive file\n");
                fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
                exit(1);
	}
	if( mtpt==NULL ) {
                fprintf(stderr, "missing mount point\n");
                fprintf(stderr, "see `%s -h' for usage\n", argv[0]);
                exit(1);
	}

	/* check if mtpt is ok and writeable */
	if( stat( mtpt, &st ) != 0 ) {
		perror( "Error stat'ing mountpoint" );
		exit( EXIT_FAILURE );
	}
	if( ! S_ISDIR( st.st_mode ) ) {
		fprintf( stderr, "Problem with mountpoint: %s\n",
				strerror( ENOTDIR ) );
		exit( EXIT_FAILURE );
	}

	/* check if archive is writeable */
	archiveFd = open( archiveFile, O_RDWR );
	if( archiveFd != -1 ) {
		archiveWriteable = 1;
		close( archiveFd );
	}
	/* open archive and read meta data */
	archiveFd = open( archiveFile, O_RDONLY );
	if( archiveFd == -1 ) {
		perror( "opening archive failed" );
		return EXIT_FAILURE;
	}
	build_tree( mtpt );

	/* save directory this was started from */
	oldpwd = open( ".", 0 );

	/* Initialize the node tree lock */
	pthread_rwlock_init(&fs->lock, NULL);

	/* now do the real mount */
	fuse_ret = fuse_main( args.argc, args.argv, &ar_oper, NULL );

	/* go back to saved dir */
	fchdir( oldpwd );

	/* save changes if modified */
	if( archiveWriteable && !options.readonly && archiveModified ) {
		if( save( archiveFile ) != 0 ) {
			fprintf( stderr, "Saving new archive failed\n" );
		}
	}

	/* clean up */
	close( archiveFd );

	return EXIT_SUCCESS;
}

#endif

/*
vim:ts=8:softtabstop=8:sw=8:noexpandtab
*/

