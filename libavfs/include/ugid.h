/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

struct ugidcache;

struct ugidcache *av_new_ugidcache();
char *av_finduname(struct ugidcache *cache, int uid, const char *deflt);
int av_finduid(struct ugidcache *cache, const char *uname, int deflt);
char *av_findgname(struct ugidcache *cache, int gid, const char *deflt);
int av_findgid(struct ugidcache *cache, const char *gname, int deflt);
