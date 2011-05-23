/*
    AVFS: A Virtual File System Library
    Copyright (C) 2010  Ralf Hoffmann <ralf@boomerangsworld.de>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.

    based on bzfile.h
*/


#include "avfs.h"

struct xzfile;
struct xzcache;

avssize_t av_xzfile_pread(struct xzfile *fil, struct xzcache *zc, char *buf,
                          avsize_t nbyte, avoff_t offset);

struct xzfile *av_xzfile_new(vfile *vf);
int av_xzfile_size(struct xzfile *fil, struct xzcache *zc, avoff_t *sizep);
struct xzcache *av_xzcache_new();
