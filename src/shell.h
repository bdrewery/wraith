#ifndef _SHELL_H
#define _SHELL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#define ERR_BINSTAT     1
#define ERR_BINMOD      2
#define ERR_PASSWD      3
#define ERR_WRONGBINDIR 4
#define ERR_DATADIR	5
#define ERR_TMPSTAT     8
#define ERR_TMPMOD      9
#define ERR_WRONGUID    12
#define ERR_WRONGUNAME  13
#define ERR_BADCONF     14
#define ERR_BADBOT	15
#define ERR_BADPASS	16
#define ERR_BOTDISABLED	17
#define ERR_NOBOTS	18
#define ERR_NOBOT	19
#define ERR_NOUSERNAME	20
#define ERR_NOHOMEDIR	21
#define ERR_NOTINIT	22
#define ERR_MAX         23

#define EMAIL_OWNERS    BIT0
#define EMAIL_TEAM      BIT1

#define DETECT_LOGIN 	1
#define DETECT_TRACE 	2
#define DETECT_PROMISC 	3
#define DETECT_PROCESS 	4		/* NOT USED */
#define DETECT_SIGCONT 	5

#define DET_IGNORE 	0
#define DET_WARN 	1
#define DET_REJECT 	2
#define DET_DIE 	3
#define DET_SUICIDE 	4

#define DETECTED_LEN	8		/* 'suicide' is longest word */

void check_maxfiles();
void check_mypid();
void clear_tmp();
char *homedir(bool = 1);
char *my_username();
char *my_uname();
#ifndef CYGWIN_HACKS
char *move_bin(const char *, const char *, bool);
#endif /* !CYGWIN_HACKS */
void expand_tilde(char **);
void baduname(char *, char *);
int email(char *, char *, int);
int shell_exec(char *, char *, char **, char **);
#ifndef CYGWIN_HACKS
void check_last();
void check_promisc();
void check_trace(int);
#ifdef NOT_USED
void check_processes();
#endif
void check_crontab();
void crontab_del();
int crontab_exists();
void crontab_create(int);
void detected(int, char *);
#endif /* !CYGWIN_HACKS */
void suicide(const char *);
#ifdef __GNUC__
 void werr(int) __attribute__((noreturn));
#else
 void werr(int);
#endif
char *werr_tostr(int);
int det_translate(const char *);
const char *det_translate_num(int);
char *shell_escape(const char *);
int mkdir_p(const char *);
extern bool		clear_tmpdir;

#endif /* _SHELL_H */
