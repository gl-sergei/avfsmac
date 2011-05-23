/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

struct proginfo {
    const char **prog;

    int ifd;
    int ofd;
    int efd;
  
    int pid;

    const char *wd;
};

void       av_init_proginfo(struct proginfo *pi);
int        av_start_prog(struct proginfo *pi);
int        av_wait_prog(struct proginfo *pi, int tokill, int check);
                    
