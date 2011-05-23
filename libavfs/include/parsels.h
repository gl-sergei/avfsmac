/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

struct lscache;

struct lscache *av_new_lscache();
int av_parse_ls(struct lscache *cache,const char *line,
                  struct avstat *stbuf, char **filename, char **linkname);

