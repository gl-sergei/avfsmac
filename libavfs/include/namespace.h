/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#define NSF_NOCASE (1 << 0) /* for case-insensitive key comparisons */

struct namespace;
struct entry;

struct namespace *av_namespace_new();
struct entry *av_namespace_lookup(struct namespace *ns, struct entry *parent,
                                    const char *name);
struct entry *av_namespace_lookup_all(struct namespace *ns, struct entry *prev,
                               const char *name);
struct entry *av_namespace_resolve(struct namespace *ns, const char *path);
char *av_namespace_getpath(struct entry *ent);
void av_namespace_set(struct entry *ent, void *data);
void *av_namespace_get(struct entry *ent);
char *av_namespace_name(struct entry *ent);
struct entry *av_namespace_next(struct entry *ent);
struct entry *av_namespace_subdir(struct namespace *ns, struct entry *ent);
struct entry *av_namespace_parent(struct entry *ent);
void av_namespace_setflags(struct entry *ent, int setflags, int resetflags);
struct entry *av_namespace_nth(struct namespace *ns, struct entry *parent,
			       unsigned int n);
