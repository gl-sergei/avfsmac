/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/
#include "avfs.h"

int av_file_open(vfile *vf, ventry *ve, int flags, avmode_t mode);
int av_file_close(vfile *vf);
avssize_t av_file_read(vfile *vf, char *buf, avsize_t nbyte);
avssize_t av_file_pread(vfile *vf, char *buf, avsize_t nbyte, avoff_t offset);
avssize_t av_file_write(vfile *vf, const char *buf, avsize_t nbyte);
avssize_t av_file_pwrite(vfile *vf, const char *buf, avsize_t nbyte,
                         avoff_t offset);
int av_file_truncate(vfile *vf, avoff_t length);
int av_file_getattr(vfile *vf, struct avstat *buf, int attrmask);
int av_file_setattr(vfile *vf, struct avstat *buf, int attrmask);
avoff_t av_file_lseek(vfile *vf, avoff_t offset, int whence);
int av_open(ventry *ve, int flags, avmode_t mode, vfile **resp);
int av_close(vfile *vf);

int av_fd_open_entry(ventry *ve, int flags, avmode_t mode);
int av_fd_open(const char *path, int flags, avmode_t mode);
int av_fd_close(int fd);
avssize_t av_fd_read(int fd, void *buf, avsize_t nbyte);
avssize_t av_fd_write(int fd, const char *buf, avsize_t nbyte);
avoff_t av_fd_lseek(int fd, avoff_t offset, int whence);
int av_fd_readdir(int fd, struct avdirent *buf, avoff_t *posp);
int av_fd_getattr(int fd, struct avstat *buf, int attrmask);
int av_fd_setattr(int fd, struct avstat *buf, int attrmask);
int av_fd_truncate(int fd, avoff_t length);
