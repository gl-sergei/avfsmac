/*
    AVFS: A Virtual File System Library
    Copyright (C) 2000-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

struct program;

int av_start_program(const char **prog, struct program **resp);
int av_program_getline(struct program *pr, char **linep, long timeoutms);
int av_program_log_output(struct program *pr);
int av_run_program(const char **prog);

/* av_program_getline() will return:
   1 and *linep != NULL  -- success
   1 and *linep == NULL  -- eof
   0                     -- timeout
   < 0                   -- error
*/

