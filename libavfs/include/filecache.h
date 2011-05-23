/*  
    AVFS: A Virtual File System Library
    Copyright (C) 1998  Miklos Szeredi <miklos@szeredi.hu>
    
    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

void *av_filecache_get(const char *key);
void av_filecache_set(const char *key, void *obj);
int av_filecache_getkey(ventry *ve, char **resp);
