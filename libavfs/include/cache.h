/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>
    Copyright (C) 2006       Ralf Hoffmann (ralf@boomerangsworld.de)

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

struct cacheobj;

void av_cache_checkspace();
void av_cache_diskfull();

/**
 * cache V1 interface using external cache objects
 * The object created by _new is not referenced by the cache
 * itself so the user has to take care of this. The object
 * is required to access to object stored in the cache.
 */
struct cacheobj *av_cacheobj_new(void *obj, const char *name);
void *av_cacheobj_get(struct cacheobj *cobj);
void av_cacheobj_setsize(struct cacheobj *cobj, avoff_t diskusage);

/**
 * cache V2 interface using internal cache objects
 * The interface doesn't returns the cache object storing
 * the obj. The name is used to access the object.
 */
int av_cache2_set(void *obj, const char *name);
void *av_cache2_get(const char *name);
void av_cache2_setsize(const char *name, avoff_t diskusage);
