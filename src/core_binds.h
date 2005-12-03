#ifndef _CORE_BINDS_H_
#define _CORE_BINDS_H_

#include "auth.h"

void core_binds_init();
int check_bind_dcc(const char *, int, const char *);
int real_check_bind_dcc(const char *, int, const char *, Auth *);
void check_bind_bot(const char *, const char *, const char *);
void check_bind_nkch(const char *, const char *);
void check_bind_away(const char *, int, const char *);
void check_bind_chon(char *, int);
void check_bind_chof(char *, int);
#endif /* !_CORE_BINDS_H */
