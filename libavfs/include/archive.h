/*
    AVFS: A Virtual File System Library
    Copyright (C) 2000-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"
#include "namespace.h"

struct archive;
struct archnode;
struct archfile;

#define ARF_NOBASE (1 << 0)

struct archparams {
    void *data;
    int flags;
    int (*parse) (void *data, ventry *ent, struct archive *arch);
    int (*open) (ventry *ve, struct archfile *fil);
    int (*close) (struct archfile *fil);
    avssize_t (*read)  (vfile *vf, char *buf, avsize_t nbyte);
    void (*release) (struct archive *arch, struct archnode *nod);
};

#define ANOF_DIRTY    (1 << 0)
#define ANOF_CREATED  (1 << 1)
#define ANOF_AUTODIR  (1 << 2)

struct archnode {
    struct avstat st;
    char *linkname;
    int flags;
    
    avoff_t offset;
    avoff_t realsize;
    
    int numopen;

    void *data;
};

struct archfile {
    vfile *basefile;
    struct archive *arch;
    struct archnode *nod;
    struct entry *ent;     /* Only for readdir */
    struct entry *curr;
    int currn;
    void *data;
};


int av_archive_init(const char *name, struct ext_info *exts, int version,
                    struct vmodule *module, struct avfs **avfsp);

avssize_t av_arch_read(vfile *vf, char *buf, avsize_t nbyte);
struct archnode *av_arch_new_node(struct archive *arch, struct entry *ent,
                                  int isdir);
void av_arch_del_node(struct entry *ent);
struct entry *av_arch_resolve(struct archive *arch, const char *path,
                              int create, int flags);
int av_arch_isroot(struct archive *arch, struct entry *ent);
struct entry *av_arch_create(struct archive *arch, const char *path,
                             int flags);

static inline struct archfile *arch_vfile_file(vfile *vf)
{
    return (struct archfile *) vf->data;
}

