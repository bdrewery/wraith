/* enclink.h
 *
 */

#ifndef _ENCLINK_H
#define _ENCLINK_H

#include <sys/types.h>

/* must leave old ones in here */
enum {
        LINK_GHOST = 0,	/* attic */
	LINK_GHOSTNAT, /* attic */
        LINK_GHOSTSHA1, /* attic */
        LINK_GHOSTMD5, /* attic */
        LINK_CLEARTEXT,
	LINK_GHOSTCASE, /* attic */
	LINK_GHOSTCASE2,
	LINK_GHOSTPRAND,
        LINK_GHOSTCLEAN
};
enum direction_t {
        FROM,
        TO
};

struct enc_link {
  const char *name;
  int type;
  void (*link) (int, direction_t);
  char *(*write) (int, char *, size_t *);
  int (*read) (int, char *, size_t *);
  void (*parse) (int, int, char *);
};

struct enc_link_dcc {
  struct enc_link *method;
  int method_number;
};

extern struct enc_link enclink[];


extern int link_find_by_type(int);

extern void link_link(int, int, int, direction_t);
extern char *link_write(int, char *, size_t *);
extern int link_read(int, char *, size_t *);
extern void link_hash(int, char *);
#ifdef __GNUC__
 extern void link_send(int, const char *, ...) __attribute__((format(printf, 2, 3)));
#else
 extern void link_send(int, const char *, ...);
#endif
extern void link_done(int);
extern void link_parse(int, char *);
extern void link_get_method(int);

#endif /* !_ENCLINK_H */
