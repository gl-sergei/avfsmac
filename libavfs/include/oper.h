/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/
#include "avfs.h"

int        av_open(ventry *ve, int flags, avmode_t mode, vfile **retp);
int        av_close(vfile *vf);
avssize_t  av_read(vfile *vf, char *buf, avsize_t nbyte);
avssize_t  av_write(vfile *vf, const char *buf, avsize_t nbyte);
avssize_t  av_pread(vfile *vf, char *buf, avsize_t nbyte,  avoff_t offset);
avssize_t  av_pwrite(vfile *vf, const char *buf, avsize_t nbyte,
                       avoff_t offset);
avoff_t    av_lseek(vfile *vf, avoff_t offset, int whence);
int        av_ftruncate(vfile *vf, avoff_t length);
int        av_getattr(ventry *ve, struct avstat *buf, int attrmask, int flags);
int        av_fgetattr(vfile *vf, struct avstat *buf, int attrmask);
int        av_fsetattr(vfile *vf, struct avstat *buf, int attrmask);
int        av_access(ventry *ve, int amode);
int        av_readlink(ventry *ve, char **bufp);
int        av_unlink(ventry *ve);
int        av_rmdir(ventry *ve);
int        av_mkdir(ventry *ve, avmode_t mode);
int        av_mknod(ventry *ve, avmode_t mode, avdev_t dev);
int        av_symlink(const char *path, ventry *newve);
int        av_rename(ventry *ve, ventry *newve);
int        av_link(ventry *ve, ventry *newve);
