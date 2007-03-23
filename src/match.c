/*
 * match.c
 *   wildcard matching functions
 *
 *
 * Once this code was working, I added support for % so that I could
 * use the same code both in Eggdrop and in my IrcII client.
 * Pleased with this, I added the option of a fourth wildcard, ~,
 * which matches varying amounts of whitespace (at LEAST one space,
 * though, for sanity reasons).
 * 
 * This code would not have been possible without the prior work and
 * suggestions of various sourced.  Special thanks to Robey for
 * all his time/help tracking down bugs and his ever-helpful advice.
 * 
 * 04/09:  Fixed the "*\*" against "*a" bug (caused an endless loop)
 * 
 *   Chris Fuller  (aka Fred1@IRC & Fwitz@IRC)
 *     crf@cfox.bchs.uh.edu
 * 
 * I hereby release this code into the public domain
 *
 */


#include "common.h"
#include "match.h"
#include "rfc1459.h"
#include "socket.h"

#define WILDS '*'  /* matches 0 or more characters (including spaces) */
#define WILDQ '?'  /* matches exactly one character */

#define NOMATCH 0
#define MATCH (match+sofar)

/* general/host matching */
int _wild_match(register unsigned char *m, register unsigned char *n)
{
  unsigned char *ma = m, *na = n;

  /* null strings should never match */
  if ((ma == 0) || (na == 0) || (!*ma) || (!*na))
    return NOMATCH;

  unsigned char *lsm = NULL, *lsn = NULL;
  int match = 1;
  register int sofar = 0;

  /* find the end of each string */
  while (*(++m));
  m--;
  while (*(++n));
  n--;

  while (n >= na) {
    /* If the mask runs out of chars before the string, fall back on
     * a wildcard or fail. */
    if (m < ma) {
      if (lsm) {
        n = --lsn;
        m = lsm;
        if (n < na) lsm = 0;
        sofar = 0;
      }
      else return NOMATCH;
    }
    switch (*m) {
    case WILDS:                /* Matches anything */
      do
        m--;                    /* Zap redundant wilds */
      while ((m >= ma) && (*m == WILDS));
      lsm = m;
      lsn = n;
      match += sofar;
      sofar = 0;                /* Update fallback pos */
      if (m < ma) return MATCH;
      continue;                 /* Next char, please */
    case WILDQ:
      m--;
      n--;
      continue;                 /* '?' always matches */
    }
    if (rfc_toupper(*m) == rfc_toupper(*n)) {   /* If matching char */
      m--;
      n--;
      sofar++;                  /* Tally the match */
      continue;                 /* Next char, please */
    }
    if (lsm) {                  /* To to fallback on '*' */
      n = --lsn;
      m = lsm;
      if (n < na)
        lsm = 0;                /* Rewind to saved pos */
      sofar = 0;
      continue;                 /* Next char, please */
    }
    return NOMATCH;             /* No fallback=No match */
  }
  while ((m >= ma) && (*m == WILDS))
    m--;                        /* Zap leftover %s & *s */
  return (m >= ma) ? NOMATCH : MATCH;   /* Start of both = match */
}

static inline int
comp_with_mask(void *addr, void *dest, unsigned int mask)
{
  if (memcmp(addr, dest, mask >> 4) == 0)
  {
    int n = mask >> 4;
    int m = ((-1) << (8 - (mask % 8)));

    if (mask % 8 == 0 ||
       (((unsigned char *) addr)[n] & m) == (((unsigned char *) dest)[n] & m))
      return (1);
  }
  return (0);
}

/* match_cidr()
 *
 * Input - mask, address
 * Ouput - + = Matched 0 = Did not match
 */

int
match_cidr(const char *s1, const char *s2)
{
  char *ipmask = strrchr(s1, '@');
  if(ipmask == NULL)
    return 0;

  char *ip = strrchr(s2, '@');
  if(ip == NULL)
    return 0;

  char mask[NICKLEN + UHOSTLEN + 6] = "";

  strlcpy(mask, s1, sizeof(mask));
  ipmask = mask + (ipmask - s1);

  char address[NICKLEN + UHOSTLEN + 6] = "";

  strlcpy(address, s2, sizeof(address));
  ip = address + (ip - s2);

  *ipmask++ = '\0';

  char *len = strrchr(ipmask, '/');
  if(len == NULL)
    return 0;

  *len++ = '\0';

  int cidrlen = atoi(len);
  if(cidrlen == 0)
    return 0;

  *ip++ = '\0';

  int ret = 0;
#ifdef USE_IPV6
  int aftype = 0;
#endif

  sockname_t ipaddr, maskaddr;
  egg_bzero(&ipaddr, sizeof(ipaddr));
  egg_bzero(&maskaddr, sizeof(maskaddr));

  if (!strchr(ip, ':') && !strchr(ipmask, ':'))
    aftype = ipaddr.family = maskaddr.family =  AF_INET;
#ifdef USE_IPV6
  else if (strchr(ip, ':') && strchr(ipmask, ':'))
    aftype = ipaddr.family = maskaddr.family = AF_INET6;
#endif /* USE_IPV6 */
  else
    return 0;

#ifdef USE_IPV6
  if (aftype == AF_INET6) {
    inet_pton(aftype, ip, &ipaddr.u.ipv6.sin6_addr);
    inet_pton(aftype, ipmask, &maskaddr.u.ipv6.sin6_addr);
    if (comp_with_mask(&ipaddr.u.ipv6.sin6_addr.s6_addr, &maskaddr.u.ipv6.sin6_addr.s6_addr, cidrlen) && 
       ((ret = wild_match(mask, address))))
      return ret;
  } else if (aftype == AF_INET) {
#endif /* USE_IPV6 */
    inet_pton(aftype, ip, &ipaddr.u.ipv4.sin_addr);
    inet_pton(aftype, ipmask, &maskaddr.u.ipv4.sin_addr);
    if (comp_with_mask(&ipaddr.u.ipv4.sin_addr.s_addr, &maskaddr.u.ipv4.sin_addr.s_addr, cidrlen) && 
       ((ret = wild_match(mask, address))))
      return ret;
#ifdef USE_IPV6
  }
#endif
  return 0;
}

