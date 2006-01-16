#ifndef _SPRINTFH_H
#define _SPRINTF_H


#include <sys/types.h>
#include <stdarg.h>

#define VSPRINTF_MAXSIZE		4096

size_t simple_vsnprintf(char *, size_t, const char *, va_list);
size_t simple_vsprintf(char *, const char *, va_list);
#ifdef __GNUC__
 size_t simple_sprintf (char *, const char *, ...) __attribute__((format(printf, 2, 3)));
 size_t simple_snprintf (char *, size_t, const char *, ...) __attribute__((format(printf, 3, 4)));
#else
 size_t simple_sprintf (char *, const char *, ...);
 size_t simple_snprintf (char *, size_t, const char *, ...);
#endif
size_t simple_snprintf2 (char *, size_t, const char *, ...);

#endif /* _SPRINTF_H */
