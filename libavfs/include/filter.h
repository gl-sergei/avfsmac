/*  
    AVFS: A Virtual File System Library
    Copyright (C) 1998  Miklos Szeredi <miklos@szeredi.hu>
    
    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

int av_init_filt(struct vmodule *module, int version, const char *name,
                 const char *prog[], const char *revprog[],
                 struct ext_info *exts, struct avfs **resp);
