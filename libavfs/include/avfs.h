/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#ifndef _AVFS_H
#define _AVFS_H

#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

/* The following 3 includes are not needed except for some
 * systems which redefine open to open64 etc. (namely Sun)
 */
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#ifndef __GNUC__
#define __attribute__(x)
#endif

typedef unsigned char       avbyte;      /* 1 byte unsigned    */
typedef unsigned short      avushort;    /* 2 bytes unsigned   */
typedef unsigned int        avuint;      /* 4 bytes unsigned   */
typedef unsigned long       avulong;     /* 4-8 bytes unsigned */
typedef long long           avquad;      /* 8 bytes signed     */
typedef unsigned long long  avuquad;     /* 8 bytes unsigned   */

typedef avuquad avdev_t;
typedef avuint  avmode_t;
typedef avuint  avnlink_t;
typedef avuint  avuid_t;
typedef avuint  avgid_t;
typedef avuquad avino_t;
typedef avquad  avoff_t;
typedef long    avtime_t;
typedef avuint  avsize_t;
typedef int     avssize_t;
typedef avulong avblksize_t;
typedef avquad  avblkcnt_t;

typedef pthread_mutex_t avmutex;

typedef struct _avtimestruc_t avtimestruc_t;
struct _avtimestruc_t {
    avtime_t sec;
    long     nsec;
};

struct avstat {
    avdev_t          dev;
    avino_t          ino;
    avmode_t         mode;
    avnlink_t        nlink;
    avuid_t          uid;
    avgid_t          gid;
    avdev_t          rdev;
    avoff_t          size;
    avblksize_t      blksize;
    avblkcnt_t       blocks;
    avtimestruc_t    atime;
    avtimestruc_t    mtime;
    avtimestruc_t    ctime;
};

struct avdirent {
    avino_t ino;
    int type;
    char *name;
};

typedef struct _ventry ventry;
struct _ventry {
    void *data;
    struct avmount *mnt;
};

typedef struct _vfile vfile;
struct _vfile {
    void *data;
    struct avmount *mnt;
    int flags;
    avoff_t ptr;
    avmutex lock;
};

struct avmount {
    ventry *base;
    struct avfs *avfs;
    char *opts;
    int flags;
};

struct avfs {
    /* private */
    struct vmodule *module;
    avmutex lock;
    avino_t inoctr;

    /* read-only: */
    char *name;
    struct ext_info *exts;
    void *data;
    int version;
    int flags;
    avdev_t dev;

    void      (*destroy) (struct avfs *avfs);
    
    int       (*lookup)  (ventry *ve, const char *name, void **retp);
    void      (*putent)  (ventry *ve);
    int       (*copyent) (ventry *ve, void **retp);
    int       (*getpath) (ventry *ve, char **retp);
                         
    int       (*access)  (ventry *ve, int amode);
    int       (*readlink)(ventry *ve, char **bufp);
    int       (*symlink) (const char *path, ventry *newve);
    int       (*unlink)  (ventry *ve);
    int       (*rmdir)   (ventry *ve);
    int       (*mknod)   (ventry *ve, avmode_t mode, avdev_t dev);
    int       (*mkdir)   (ventry *ve, avmode_t mode);
    int       (*rename)  (ventry *ve, ventry *newve);
    int       (*link)    (ventry *ve, ventry *newve);
                         
    int       (*open)    (ventry *ve, int flags, avmode_t mode, void **retp);
    int       (*close)   (vfile *vf);
    avssize_t (*read)    (vfile *vf, char *buf, avsize_t nbyte);
    avssize_t (*write)   (vfile *vf, const char *buf, avsize_t nbyte);
    int       (*readdir) (vfile *vf, struct avdirent *buf);
    int       (*getattr) (vfile *vf, struct avstat *buf, int attrmask);
    int       (*setattr) (vfile *vf, struct avstat *buf, int attrmask);
    int       (*truncate)(vfile *vf, avoff_t length);
    avoff_t   (*lseek)   (vfile *vf, avoff_t offset, int whence);
};

struct ext_info {
    const char *from;
    const char *to;
};

struct avtm {
    int sec;    /* [0-59] (note: 61 _can_ happen) */
    int min;    /* [0-59] */
    int hour;   /* [0-23] */
    int day;    /* [1-31] */
    int mon;    /* [0-11] */
    int year;   /* Year - 1900 */
};

typedef struct {
    char *name;
    int is_tmp;
} real_file;


typedef struct {
    int outfd;
    ventry *ve;
} rep_file;

#define AV_NEW(ptr)   ptr = av_calloc(sizeof(*(ptr)))

#define AV_NEW_OBJ(ptr, destr) \
   ptr = av_new_obj(sizeof(*(ptr)), (void (*)(void *)) destr)

#define AV_LOCK_DECL(mutex) avmutex mutex
#define AV_INITLOCK(mutex) pthread_mutex_init(&(mutex), NULL)
#define AV_FREELOCK(mutex) pthread_mutex_destroy(&(mutex));
#define AV_LOCK(mutex)     pthread_mutex_lock(&(mutex))
#define AV_UNLOCK(mutex)   pthread_mutex_unlock(&(mutex))

#define AV_INIT_EXT(e, f, t) (e).from = (f), (e).to = (t)

#define AV_MAX(x, y) ((x) > (y) ? (x) : (y))
#define AV_MIN(x, y) ((x) < (y) ? (x) : (y))
#define AV_DIV(x, y) ((x) ? (((x) - 1) / (y) + 1) : 0)

#define AV_TIME_LESS(t1, t2) ((t1).sec < (t2).sec || \
                              ((t1).sec == (t2).sec && (t1).nsec < (t2).nsec))

#define AV_TIME_EQ(t1, t2) ((t1).sec == (t2).sec && (t1).nsec == (t2).nsec)

#define AV_BLOCKS(x) AV_DIV(x, 512)

#define AV_MAXTIME (~(1L << (sizeof(avtime_t) * 8 - 1)))
#define AV_MAXOFF  0x7FFFFFFFFFFFFFFFLL

#define AV_DIR_SEP_CHAR '/'
#define AV_DIR_SEP_STR  "/"

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define AVSEEK_SET   0
#define AVSEEK_CUR   1
#define AVSEEK_END   2

#define AVR_OK       4
#define AVW_OK       2
#define AVX_OK       1
#define AVF_OK       0

#define AVO_ACCMODE  0x03

#define AVO_RDONLY   0
#define AVO_WRONLY   1
#define AVO_RDWR     2
#define AVO_NOPERM   3

#define AV_ISWRITE(flags) ((flags & AVO_ACCMODE) == AVO_WRONLY || \
                           (flags & AVO_ACCMODE) == AVO_RDWR)

#define AVO_CREAT      0x00000040
#define AVO_EXCL       0x00000080
#define AVO_TRUNC      0x00000200
#define AVO_APPEND     0x00000400
#define AVO_NONBLOCK   0x00000800
#define AVO_SYNC       0x00001000
#define AVO_DIRECTORY  0x00010000
#define AVO_NOFOLLOW   0x00020000

#define AVA_DEV     (1 << 0)
#define AVA_INO     (1 << 1)
#define AVA_MODE    (1 << 2)
#define AVA_NLINK   (1 << 3)
#define AVA_UID     (1 << 4)
#define AVA_GID     (1 << 5)
#define AVA_RDEV    (1 << 6)
#define AVA_SIZE    (1 << 7)
#define AVA_BLKSIZE (1 << 8)
#define AVA_BLKCNT  (1 << 9)
#define AVA_ATIME   (1 << 10)
#define AVA_MTIME   (1 << 11)
#define AVA_CTIME   (1 << 12)

#define AVA_ALL     0x00001fff

#define AV_IFMT   0170000
#define AV_IFDIR  0040000
#define AV_IFCHR  0020000
#define AV_IFBLK  0060000
#define AV_IFREG  0100000
#define AV_IFIFO  0010000
#define AV_IFLNK  0120000
#define AV_IFSOCK 0140000

#define AV_ISVTX  01000
#define AV_ISGID  02000
#define AV_ISUID  04000

#define AV_ISTYPE(mode, mask)  (((mode) & AV_IFMT) == (mask))
 
#define AV_ISDIR(mode)  AV_ISTYPE((mode), AV_IFDIR)
#define AV_ISCHR(mode)  AV_ISTYPE((mode), AV_IFCHR)
#define AV_ISBLK(mode)  AV_ISTYPE((mode), AV_IFBLK)
#define AV_ISREG(mode)  AV_ISTYPE((mode), AV_IFREG)
#define AV_ISFIFO(mode) AV_ISTYPE((mode), AV_IFIFO)
#define AV_ISLNK(mode)  AV_ISTYPE((mode), AV_IFLNK)
#define AV_ISSOCK(mode) AV_ISTYPE((mode), AV_IFSOCK)

#define AV_TYPE(mode) (((mode) & AV_IFMT) >> 12)

#define AVLOG_ERROR     001
#define AVLOG_WARNING   002
#define AVLOG_DEBUG     004
#define AVLOG_SYSCALL   010

#define AVF_NEEDSLASH  (1 << 0)
#define AVF_ONLYROOT   (1 << 1)
#define AVF_NOLOCK     (1 << 2)

int        av_new_avfs(const char *name, struct ext_info *exts, int version,
                       int flags, struct vmodule *module, struct avfs **retp);
void       av_add_avfs(struct avfs *avfs);
avino_t    av_new_ino(struct avfs *avfs);

int        av_check_version(const char *modname, const char *name, int version,
                            int need_ver, int provide_ver);

int        av_copy_ventry(ventry *ve, ventry **retp);
void       av_free_ventry(ventry *ve);
int        av_generate_path(ventry *ve, char **pathp);

avdev_t    av_mkdev(int major, int minor);
void       av_splitdev(avdev_t dev, int *majorp, int *minorp);
void       av_default_stat(struct avstat *stbuf);
          
void       av_log(int level, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
char      *av_get_config(const char *param);
void      *av_malloc(avsize_t nbyte);
void      *av_calloc(avsize_t nbyte);
void      *av_realloc(void *ptr, avsize_t nbyte);
void       av_free(void *ptr);
         
void      *av_new_obj(avsize_t nbyte, void (*destr)(void *));
void       av_ref_obj(void *obj);
void       av_unref_obj(void *obj);
          
char      *av_strdup(const char *s);
char      *av_strndup(const char *s, avsize_t len);
char      *av_stradd(char *s1, ...);
          
void       av_registerfd(int fd);
void       av_curr_time(avtimestruc_t *tim);
avtime_t   av_time();
void       av_sleep(unsigned long msec);
avtime_t   av_mktime(struct avtm *tp);
void       av_localtime(avtime_t t, struct avtm *tp);
int        av_get_tmpfile(char **retp);
void       av_del_tmpfile(char *tmpfile);
avoff_t    av_tmp_free();

avssize_t  av_read_all(vfile *vf, char *buf, avsize_t nbyte);
avssize_t av_pread_all(vfile *vf, char *buf, avsize_t nbyte, avoff_t offset);

#endif /* _AVFS_H */
