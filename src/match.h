#ifndef _MATCH_H
#define _MATCH_H


/*
int wild_match(register unsigned char *, register unsigned char *);
*/

#define wild_match(a,b) _wild_match((unsigned char *)(a),(unsigned char *)(b))

int _wild_match(register unsigned char *, register unsigned char *);

//int wild_match(register char *, register char *);

int match_cidr(const char *, const char *);

#endif /* !_MATCH_H */
