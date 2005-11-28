#ifndef _RFC1459_H
#define _RFC1459_H

int _rfc_casecmp(const char *, const char *);
int _rfc_toupper(int);
char *rfc_strtoupper(char *);

extern int (*rfc_casecmp) (const char *, const char *);
extern int (*rfc_toupper) (int);

#endif /* !_RFC1459_H */
