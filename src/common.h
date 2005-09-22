/*
 * common.h
 *   include file to include most other include files
 *
 */

#ifndef _COMMON_H
#define _COMMON_H

/* These should be in a common.h, like it or not... */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "bits.h"
#include "garble.h"
#include "sprintf.h"
#include "conf.h"
#include "debug.h"
#include "eggdrop.h"
#include "flags.h"
#include "log.h"
#include "dccutil.h"
#include "chan.h"
#include "compat/compat.h"
#include "ptrlist.h"

#ifdef CYGWIN_HACKS
#  include <windows.h>
#endif /* CYGWIN_HACKS */
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include "lang.h"


#ifdef WIN32
# undef exit
# define exit(x) ExitProcess(x)
#endif /* WIN32 */


//# undef system
int system(const char *);		
//# define system(_run) 	my_system(_run)

#endif				/* _COMMON_H */
