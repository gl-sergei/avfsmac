/*
    AVFS: A Virtual File System Library
    Copyright (C) 1998-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#include "avfs.h"

#define FILEBUF_NONBLOCK (1 << 0)
#define FILEBUF_WRITE    (1 << 1)

struct filebuf;

struct filebuf *av_filebuf_new(int fd, int flags);
int av_filebuf_eof(struct filebuf *fb);
int av_filebuf_check(struct filebuf *fbs[], unsigned int numfbs,
                     long timeoutms);

int av_filebuf_readline(struct filebuf *fb, char **linep);
int av_filebuf_getline(struct filebuf *fb, char **linep, long timeoutms);
avssize_t av_filebuf_read(struct filebuf *fb, char *buf, avsize_t nbytes);
avssize_t av_filebuf_write(struct filebuf *fb, const char *buf,
                           avsize_t nbytes);


/* av_filebuf_getline() will return:
   1 and *linep != NULL  -- success
   1 and *linep == NULL  -- eof
   0                     -- timeout
   < 0                   -- read error
*/
