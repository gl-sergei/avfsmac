/*
    AVFS: A Virtual File System Library
    Copyright (C) 2000-2001  Miklos Szeredi <miklos@szeredi.hu>

    This program can be distributed under the terms of the GNU GPL.
    See the file COPYING.
*/

#ifndef INCLUDED_PASSWORDS_H
#define INCLUDED_PASSWORDS_H 1

#include "avfs.h"
#include "state.h"

#define USER_SEP_STR  "@"
#define USER_SEP_CHAR (USER_SEP_STR[0])

/**
 * Note: FTP uses "user@host" or just "user@" for the account key,
 * since the username is passed in as part of the path. In the FTP
 * case the "username" member in this struct can be left NULL.
 * 
 * HTTP/DAV uses "realm@host" or just "@host", and stores the username
 * as well as the password in this struct.
 */
struct pass_session {
    char *account;
    char *username;
    char *password;
    struct pass_session *next;
    struct pass_session *prev;
};

/* these two are only needed for HTTP (see comment above) */
extern int pass_username_get(struct entry *ent, const char *param, char **resp);

extern int pass_username_set(struct entry *ent, const char *param, const char *val);

extern int pass_password_get(struct entry *ent, const char *param, char **resp);

extern int pass_password_set(struct entry *ent, const char *param, const char *val);

extern int pass_loggedin_get(struct entry *ent, const char *param, char **resp);

extern int pass_loggedin_set(struct entry *ent, const char *param, const char *val);

struct pass_session *pass_get_password(struct pass_session *passd,
    			const char *host, const char *user);

void pass_remove_session(struct pass_session *fts);

#endif /* INCLUDED_PASSWORDS_H */
