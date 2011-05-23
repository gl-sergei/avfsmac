/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998  Miklos Szeredi <miklos@szeredi.hu>
    
    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

struct realfile {
    char *name;
    int is_tmp;
};

int av_get_realfile(ventry *ve, struct realfile **resp);
