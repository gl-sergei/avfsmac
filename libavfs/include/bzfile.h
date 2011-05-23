/*
    AVFS: A Virtual File System Library
    Copyright (C) 2000-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/


#include "avfs.h"

struct bzfile;
struct bzcache;

avssize_t av_bzfile_pread(struct bzfile *fil, struct bzcache *zc, char *buf,
                          avsize_t nbyte, avoff_t offset);

struct bzfile *av_bzfile_new(vfile *vf);
int av_bzfile_size(struct bzfile *fil, struct bzcache *zc, avoff_t *sizep);
struct bzcache *av_bzcache_new();
