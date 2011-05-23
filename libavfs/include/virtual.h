/*  
    AVFS: A Virtual File System Library
    Copyright (C) 1998-1999  Miklos Szeredi <miklos@szeredi.hu>
    
    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#ifndef _VIRTUAL_H
#define _VIRTUAL_H

#include <sys/types.h>
#include <dirent.h>
#include <utime.h>
#include <sys/stat.h>

/*
  some fs functions which still need virtual equivalents:
  fchdir, statfs, sync
*/

int            virt_stat      (const char *path, struct stat *buf);
int            virt_lstat     (const char *path, struct stat *buf);
int            virt_access    (const char *path, int amode);
int            virt_readlink  (const char *path, char *buf, size_t bsiz);
int            virt_truncate  (const char *path, off_t length);
int            virt_utime     (const char *path, struct utimbuf *buf);
int            virt_chmod     (const char *path, mode_t mode);
int            virt_chown     (const char *path, uid_t owner, gid_t grp);
int            virt_lchown    (const char *path, uid_t owner, gid_t grp);

int            virt_unlink    (const char *path);
int            virt_rmdir     (const char *path);
int            virt_mkdir     (const char *path, mode_t mode);
int            virt_mknod     (const char *path, mode_t mode, dev_t dev);
int            virt_symlink   (const char *path, const char *newpath);
int            virt_rename    (const char *path, const char *newpath);
int            virt_link      (const char *path, const char *newpath);
int            virt_fstat     (int fh, struct stat *buf);
int            virt_ftruncate (int fh, off_t length);
int            virt_fchmod    (int fh, mode_t mode);
int            virt_fchown    (int fh, uid_t owner, gid_t grp);

int            virt_open      (const char *path, int flags, mode_t mode);
int            virt_close     (int fh);
ssize_t        virt_read      (int fh, void *buf, size_t nbyte);
ssize_t        virt_write     (int fh, const void *buf, size_t nbyte);
off_t          virt_lseek     (int fh, off_t offset, int whence);

DIR           *virt_opendir   (const char *path);
int            virt_closedir  (DIR *dirp);
struct dirent *virt_readdir   (DIR *dirp);
void           virt_rewinddir (DIR *dirp);

int            virt_remove    (const char *path);
int            virt_islocal   (const char *path);

#endif /* _VIRTUAL_H */
