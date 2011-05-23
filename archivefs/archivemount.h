/*
 *  archivemount.h
 *  ArchiveFS
 *
 *  Created by Юрий Гагарин on 10/18/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

/*******************/
/* data structures */
/*******************/

typedef struct node {
	struct node *parent;
	struct node *prev; /* previous in same directory */
	struct node *next; /* next in same directory */
	struct node *child; /* first child for directories */
	char *name; /* fully qualified with prepended '/' */
	char *location; /* location on disk for new/modified files, else NULL */
	int namechanged; /* true when file was renamed */
	struct archive_entry *entry; /* libarchive header data */
	int modified; /* true when node was modified */
} NODE;


/***********/
/* globals */
/***********/

typedef struct {
	int nobackup;
	int readonly;
} archive_fs_options;

typedef struct {
	
	int archiveFd; /* file descriptor of archive file, just to keep the
						   beast alive in case somebody deletes the file while
						   it is mounted */
	int archiveModified;
	int archiveWriteable;
	NODE *root;
	char *mtpt;
	char *archiveFile;
	pthread_rwlock_t lock; /* global node tree lock */
	archive_fs_options options;
	
} archive_fs_t;

typedef int (^fill_dir_t) (const char* name, struct stat* st);

int ar_init( archive_fs_t* fs, const char *archiveFile, const char* mtpt );
int ar_free( archive_fs_t *fs );
int ar_read( archive_fs_t* fs, const char *path, char *buf, size_t size, off_t offset );
int ar_getattr( archive_fs_t* fs, const char *path, struct stat *stbuf );
int ar_mkdir( archive_fs_t* fs, const char *path, mode_t mode );
int ar_rmdir( archive_fs_t* fs, const char *path );
int ar_symlink( archive_fs_t* fs, const char *from, const char *to );
int ar_link( archive_fs_t* fs, const char *from, const char *to );
int ar_truncate( archive_fs_t* fs, const char *path, off_t size );
int ar_write( archive_fs_t* fs, const char *path, const char *buf, size_t size,
			 off_t offset );
int ar_mknod( archive_fs_t* fs, const char *path, mode_t mode, dev_t rdev );
int ar_unlink( archive_fs_t* fs, const char *path );
int ar_chmod( archive_fs_t* fs, const char *path, mode_t mode );
int ar_chown( archive_fs_t* fs, const char *path, uid_t uid, gid_t gid );
int ar_utime( archive_fs_t* fs, const char *path, struct utimbuf *buf );
int ar_statfs( archive_fs_t* fs, const char *path, struct statvfs *stbuf );
int ar_rename( archive_fs_t* fs, const char *from, const char *to );
int ar_fsync( archive_fs_t* fs, const char *path, int isdatasync );
int ar_readlink( archive_fs_t* fs, const char *path, char *buf, size_t size );
int ar_open( archive_fs_t* fs, const char *path, int flags );
int ar_release( archive_fs_t* fs, const char *path );
int ar_readdir( archive_fs_t* fs, const char *path, void *buf, fill_dir_t filler,
			   off_t offset );
int ar_create( archive_fs_t* fs, const char *path, mode_t mode );
