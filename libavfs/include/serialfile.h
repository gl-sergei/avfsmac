/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

#define SFILE_NOCACHE (1 << 0)

struct sfilefuncs {
    int       (*startget) (void *data, void **resp);
    avssize_t (*read)     (void *data, char *buf, avsize_t nbyte);
    int       (*startput) (void *data, void **resp);
    avssize_t (*write)    (void *data, const char *buf, avsize_t nbyte);
    int       (*endput)   (void *data);
};

struct sfile;

struct sfile *av_sfile_new(const struct sfilefuncs *func, void *data,
			   int flags);
avssize_t av_sfile_pread(struct sfile *fil, char *buf, avsize_t nbyte,
			 avoff_t offset);
avssize_t av_sfile_pwrite(struct sfile *fil, const char *buf, avsize_t nbyte,
			  avoff_t offset);
avoff_t av_sfile_size(struct sfile *fil);
int av_sfile_truncate(struct sfile *fil, avoff_t length);
int av_sfile_startget(struct sfile *fil);
int av_sfile_flush(struct sfile *fil);
void *av_sfile_getdata(struct sfile *fil);
avoff_t av_sfile_diskusage(struct sfile *fil);
