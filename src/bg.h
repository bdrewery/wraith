#ifndef _BG_H
#define _BG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

extern time_t 		lastfork;
#if !defined(CYGWIN_HACKS) && !defined(__sun__)
extern pid_t 		watcher;
#endif

pid_t do_fork();
int close_tty();
void writepid(const char *, pid_t);
void cloak_process(int, char **);
void go_background_and_write_pid();
void check_if_already_running();

#endif /* !_BG_H */
