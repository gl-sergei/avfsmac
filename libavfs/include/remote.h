/*
    AVFS: A Virtual File System Library
    Copyright (C) 2000-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"
#define REM_LIST_SINGLE (1 << 0)
#define REM_LIST_PARENT (1 << 1)

struct remdirent {
    char *name;
    char *linkname;
    struct avstat attr;
};

struct remhostpath {
    char *host;
    char *path;
};

struct remdirlist {
    int flags;
    struct remhostpath hostpath;
    avsize_t num;
    struct remdirent *ents;
};

struct remgetparam {
    struct remhostpath hostpath;
    char *localname;
    void *data;
};

#define REM_DIR_ONLY (1 << 0)
#define REM_NOCASE   (1 << 1)

struct remote {
    void *data;
    char *name;
    int flags;

    int (*list) (struct remote *rem, struct remdirlist *dl);
    int (*get) (struct remote *rem, struct remgetparam *gp);
    int (*wait) (struct remote *rem, void *data, avoff_t end);
    void (*destroy) (struct remote *rem);
};

int av_remote_init(struct vmodule *module, struct remote *rem,
                     struct avfs **resp);
void av_remote_add(struct remdirlist *dl, const char *name,
                     const char *linkname, struct avstat *attr);
