#ifndef _USERENT_H
#define _USERENT_H

#include "users.h"

void update_mod(char *, char *, char *, char *);
void list_type_kill(struct list_type *);
void stats_add(struct userrec *, int, int);
void init_userent(void);
char *encpass(char *pass);

#endif /* !_USERENT_H */
