#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/*
 * Undefine this to completely disable context debugging.
 * WARNING: DO NOT send in bug reports if you undefine this!
 */

#undef DEBUG_CONTEXT

/*
 *    Handy aliases for memory tracking and core dumps
 */
#ifdef DEBUG_CONTEXT
#  define Context               eggContext(__FILE__, __LINE__)
#  define ContextNote(note)     eggContextNote(__FILE__, __LINE__, note)
#else
#  define Context               {}
#  define ContextNote(note)     {}
#endif

#define debug0(x)               putlog(LOG_DEBUG,"*",x)
#define debug1(x,a1)            putlog(LOG_DEBUG,"*",x,a1)
#define debug2(x,a1,a2)         putlog(LOG_DEBUG,"*",x,a1,a2)
#define debug3(x,a1,a2,a3)      putlog(LOG_DEBUG,"*",x,a1,a2,a3)
#define debug4(x,a1,a2,a3,a4)   putlog(LOG_DEBUG,"*",x,a1,a2,a3,a4)


extern bool		sdebug, segfaulted;

void stackdump(int);
void setlimits();
#ifdef __GNUC__
 void sdprintf (const char *, ...) __attribute__((format(printf, 1, 2)));
#else
 void sdprintf (const char *, ...);
#endif
char* hexize(const unsigned char* data, size_t len);
void init_signals();
void init_debug();
void eggContext(const char *, int);
void eggContextNote(const char *, int, const char *);
void printstr(unsigned char *, int);

#endif /* !_DEBUG_H */
