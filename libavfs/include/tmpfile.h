/*
    AVFS: A Virtual File System Library
    Copyright (C) 2006  Ralf Hoffmann (ralf@boomerangsworld.de)

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#ifndef _AVFS_TMPFILE_H
#define _AVFS_TMPFILE_H

#include "avfs.h"

avoff_t av_tmpfile_blksize(const char *tmpf);

#endif
