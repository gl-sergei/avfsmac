/*
    AVFS: A Virtual File System Library
    Copyright (C) 2000-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/


#include "avfs.h"

struct zfile;
struct zcache;

avssize_t av_zfile_pread(struct zfile *fil, struct zcache *zc, char *buf,
                         avsize_t nbyte, avoff_t offset);
int av_zfile_size(struct zfile *fil, struct zcache *zc, avoff_t *sizep);

struct zfile *av_zfile_new(vfile *vf, avoff_t dataoff, avuint crc, int calccrc);
struct zcache *av_zcache_new();
avoff_t av_zcache_size(struct zcache *zc);
