/* enclink.c
 *
 */


#include "common.h"
#include "enclink.h"
#include "dcc.h"
#include "net.h"
#include "misc.h"
#include "base64.h"
#include "crypto/aes_util.h"

#include <stdarg.h>
#include <signal.h>

#ifdef DEBUG
#define DEBUG_ENCLINK 1
#endif 
static void ghost_link_case(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (snum >= 0) {
    char initkey[33] = "", *tmp2 = NULL;
    char tmp[256] = "";
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    size_t key_len = 0;
    port_t port = 0;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      key_len = sizeof(socklist[snum].ikey);
      nick1 = strdup(dcc[idx].nick);
      nick2 = strdup(conf.bot->nick);
      port = htons(dcc[idx].port);
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      key_len = sizeof(socklist[snum].okey);
      nick1 = strdup(conf.bot->nick);
      nick2 = strdup(dcc[idx].nick);

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      egg_bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      port = sa.sin_port;
    }

    /* initkey-gen */
    /* bdhash port mynick conf.bot->nick */
    sprintf(tmp, "%s@%4x@%s@%s", settings.bdhash, port, strtoupper(nick1), strtoupper(nick2));
    free(nick1);
    free(nick2);
    strlcpy(keyp, SHA1(tmp), key_len);
    putlog(LOG_DEBUG, "@", "Link hash for %s: %s", dcc[idx].nick, tmp);
    putlog(LOG_DEBUG, "@", "outkey (%d): %s", strlen(keyp), keyp);

    if (direction == FROM) {
      make_rand_str(initkey, 32);       /* set the initial out/in link key to random chars. */
      socklist[snum].iseed = socklist[snum].oseed = random();
#ifdef DEBUG_ENCLINK
sdprintf("sock: %d seed: %-10lu %s", snum, socklist[snum].oseed, hexize((unsigned char*) initkey, sizeof(initkey) - 1));
#endif
      tmp2 = encrypt_string(settings.salt2, initkey);
      putlog(LOG_BOTS, "*", "Sending encrypted link handshake to %s...", dcc[idx].nick);

      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;

      link_send(idx, "elink %s %lu\n", tmp2, socklist[snum].oseed);
      free(tmp2);
      strlcpy(socklist[snum].okey, initkey, sizeof(socklist[snum].okey));
      strlcpy(socklist[snum].ikey, initkey, sizeof(socklist[snum].ikey));
    } else {
      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;
    }
  } else {
    putlog(LOG_MISC, "*", "Couldn't find socket for %s connection?? Shouldn't happen :/", dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

static inline long
prand(long *seed, int range)
{
  long long i1 = *seed;

  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  return ((i1 * range) >> 32);
}

static inline long
Prand(long *seed, int range)
{
  long long i1 = *seed;

  i1 = (i1 * 0x08088405 + 1) & 0xFFFFFFFF;
  *seed = i1;
  return (i1 * range);
}

static inline void ghost_cycle_key_in(int snum) {
  if (socklist[snum].iseed) {
    for (int i = 0; i < 16; i += sizeof(long))
      *(long *) &(socklist[snum].ikey)[i] = prand(&(socklist[snum].iseed), 0xFFFFFFFF);

    if (!socklist[snum].iseed)
      ++socklist[snum].iseed;
  }
}

static inline void ghost_cycle_key_out(int snum) {
  if (socklist[snum].oseed) {
    for (int i = 0; i < 16; i += sizeof(long))
      *(long *) &(socklist[snum].okey)[i] = prand(&(socklist[snum].oseed), 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        ++socklist[snum].oseed;
  }
}

static inline void ghost_cycle_key_in_Prand(int snum) {
  if (socklist[snum].iseed) {
#ifdef DEBUG_ENCLINK
    sdprintf("CYCLING IKEY ON %d", snum);
#endif
    for (size_t i = 0; i < (sizeof(socklist[snum].ikey) - 1); i += sizeof(long))
      *(long*) &(socklist[snum].ikey)[i] = Prand(&(socklist[snum].iseed), 0xFFFFFFFF);

    if (!socklist[snum].iseed)
      ++socklist[snum].iseed;
  }
}

static inline void ghost_cycle_key_out_Prand(int snum) {
  if (socklist[snum].oseed) {
#ifdef DEBUG_ENCLINK
    sdprintf("CYCLING OKEY ON %d", snum);
#endif
    for (size_t i = 0; i < (sizeof(socklist[snum].okey) - 1); i += sizeof(long))
      *(long*) &(socklist[snum].okey)[i] = Prand(&(socklist[snum].oseed), 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        ++socklist[snum].oseed;
  }
}

static int ghost_read(int snum, char *src, size_t *len)
{
  char *line = decrypt_string(socklist[snum].ikey, src);

  strcpy(src, line);
  free(line);
  ghost_cycle_key_in(snum);
//  *len = strlen(src);
  return OK;
}

static char *ghost_write(int snum, char *src, size_t *len)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  size_t bufpos = 0;

  srcbuf = (char *) my_calloc(1, *len + 9 + 1);
  strcpy(srcbuf, src);
  line = srcbuf;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_string(socklist[snum].okey, line);

    ghost_cycle_key_out(snum);

    buf = (char *) my_realloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
    bufpos = strlen(buf);
    line = eol;
    eol = strchr(line, '\n');
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    ghost_cycle_key_out(snum);
    buf = (char *) my_realloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    /* FIXME: technically, no \n was provided, so adding this can break checksum/size checking and cause mismatches.. */
    strcat(buf, "\n");
  }
  free(srcbuf);

  *len = strlen(buf);
  return buf;
}

static int ghost_Prand_read(int snum, char *src, size_t *len)
{
  char *b64 = b64dec((unsigned char*) src, len);
  char *line = (char *) decrypt_binary(socklist[snum].ikey, (unsigned char*) b64, len);
  free(b64);

#ifdef DEBUG_ENCLINK
  char *p = strchr(line, ' ');
  *(p++) = 0;
  size_t real_len = atoi(line);
  *len = strlen(p);

  if (real_len != *len) {
    putlog(LOG_MISC, "*", "Encrypt MISMATCH %d != %d", real_len, *len);
    sdprintf("Encrypt MISMATCH %d != %d\n", real_len, *len);
    sdprintf("mismatch");
  }

  strlcpy(src, p, *len + 1);
#else
  strlcpy(src, line, *len + 1);
#endif
  free(line);

#ifdef DEBUG_ENCLINK
  sdprintf("SEED: %-10lu IKEY: %s", socklist[snum].iseed, hexize((unsigned char*) socklist[snum].ikey, 32));
  sdprintf("READ: %s", src);
#endif

  ghost_cycle_key_in_Prand(snum);
  return OK;
}

static char *ghost_Prand_write(int snum, char *src, size_t *len)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  unsigned char *edata = NULL;
  size_t bufpos = 0, llen = 0;

#ifdef DEBUG_ENCLINK
  sdprintf("SEED: %-10lu OKEY: %s", socklist[snum].oseed, hexize((unsigned char*) socklist[snum].okey, 32));
  sdprintf("WRITE: %s", src);
#endif

  srcbuf = (char *) my_calloc(1, *len + 5 + 1);

#ifdef DEBUG_ENCLINK
  /* Add length at beginning to be checked */
  simple_snprintf(srcbuf, *len + 5 + 1, "%d %s", *len - 1, src);
  char *p = strchr(srcbuf, ' ');
  *len += (p - srcbuf) + 1;
#else
  strlcpy(srcbuf, src, *len + 1);
#endif

  line = srcbuf;

  eol = strchr(line, '\n');
  while (eol) {
    llen = eol - line;
    *eol++ = 0;
    edata = encrypt_binary(socklist[snum].okey, (unsigned char*) line, &llen);
    eline = b64enc(edata, &llen);
    free(edata);
    ghost_cycle_key_out_Prand(snum);
 
    buf = (char *) my_realloc(buf, bufpos + llen + 1 + 1);
    strncpy((char *) &buf[bufpos], eline, llen);
    free(eline);
    bufpos += llen;
    buf[bufpos++] = '\n';

    line = eol;
    eol = strchr(line, '\n');
    *len -= llen;
  }
  if (line[0]) { /* leftover line? */
    llen = *len;
    edata = encrypt_binary(socklist[snum].okey, (unsigned char*) line, &llen);
    eline = b64enc(edata, &llen);
    free(edata);

    ghost_cycle_key_out_Prand(snum);
    buf = (char *) my_realloc(buf, bufpos + llen + 1 + 1);
    strncpy((char *) &buf[bufpos], eline, llen);
    free(eline);
    bufpos += llen;
    /* FIXME: technically, no \n was provided, so adding this can break checksum/size checking and cause mismatches.. */
    buf[bufpos++] = '\n';
  }
  free(srcbuf);

  buf[bufpos] = 0;
  *len = bufpos;

  return buf;
}


void ghost_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!egg_strcasecmp(code, "elink")) {
    char *tmp = decrypt_string(settings.salt2, newsplit(&buf));

    strlcpy(socklist[snum].okey, tmp, sizeof(socklist[snum].okey));
    strlcpy(socklist[snum].ikey, tmp, sizeof(socklist[snum].ikey));
    socklist[snum].iseed = socklist[snum].oseed = atol(buf);

#ifdef DEBUG_ENCLINK
sdprintf("sock: %d seed: %-10lu %s", snum, socklist[snum].oseed, hexize((unsigned char*) socklist[snum].okey, sizeof(socklist[snum].okey) - 1));
#endif
    putlog(LOG_BOTS, "*", "Handshake with %s succeeded, we're linked.", dcc[idx].nick);
    free(tmp);
    link_done(idx);
  }
}

void ghost_Prand_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!egg_strcasecmp(code, "elink")) {
    char *tmp = decrypt_string(settings.salt2, newsplit(&buf));

    socklist[snum].iseed = socklist[snum].oseed = atol(buf);

    /* The hub/sender cycled the oseed once to send us this information.. so cycle/shift our iseed to match */
    /* This operation was only on the seed, not the key, so do it before setting the key */
    ghost_cycle_key_in_Prand(snum);

    strlcpy(socklist[snum].okey, tmp, sizeof(socklist[snum].okey));
    strlcpy(socklist[snum].ikey, tmp, sizeof(socklist[snum].ikey));

#ifdef DEBUG_ENCLINK
sdprintf("sock: %d seed: %-10lu %s", snum, socklist[snum].oseed, hexize((unsigned char*) socklist[snum].okey, sizeof(socklist[snum].okey) - 1));
#endif
    putlog(LOG_BOTS, "*", "Handshake with %s succeeded, we're linked.", dcc[idx].nick);
    free(tmp);
    link_done(idx);
  }
}

#ifdef no
static int binary_read(int snum, char *src, size_t *len)
{
  char *line = NULL;

  line = decrypt_binary(socklist[snum].ikey, (unsigned char *) src, len);
  strlcpy(src, line, SGRAB + 10);
  free(line);
  if (socklist[snum].iseed) {
    *(dword *) & socklist[snum].ikey[0] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[4] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[8] = prand(&socklist[snum].iseed, 0xFFFFFFFF);
    *(dword *) & socklist[snum].ikey[12] = prand(&socklist[snum].iseed, 0xFFFFFFFF);

    if (!socklist[snum].iseed)
      ++socklist[snum].iseed;
  }
//  *len = strlen(src);
  return OK;
}

static char *binary_write(int snum, char *src, size_t *len)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  size_t bufpos = 0;

  srcbuf = (char *) my_calloc(1, *len + 9 + 1);
  strcpy(srcbuf, src);
  line = srcbuf;

  eol = strchr(line, '\n');
  while (eol) {
    *eol++ = 0;
    eline = encrypt_binary(socklist[snum.okey, (unsigned char *) line, len);
    if (socklist[snum].oseed) {
      *(dword *) & socklist[snum].okey[0] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[8] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[12] = prand(&socklist[snum].oseed, 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        ++socklist[snum].oseed;
    }
    buf = (char *) my_realloc(buf, bufpos + len + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
    bufpos = strlen(buf);
    line = eol;
    eol = strchr(line, '\n');
  }
  if (line[0]) {
    eline = encrypt_string(socklist[snum].okey, line);
    if (socklist[snum].oseed) {
      *(dword *) & socklist[snum].okey[0] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[4] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[8] = prand(&socklist[snum].oseed, 0xFFFFFFFF);
      *(dword *) & socklist[snum].okey[12] = prand(&socklist[snum].oseed, 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        ++socklist[snum].oseed;
    }
    buf = (char *) my_realloc(buf, bufpos + strlen(eline) + 1 + 9);
    strcpy((char *) &buf[bufpos], eline);
    free(eline);
    strcat(buf, "\n");
  }
  free(srcbuf);

  *len = strlen(buf);
  return buf;
}
#endif

void link_send(int idx, const char *format, ...)
{
  char s[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s) - 1, format, va);
  va_end(va);
  remove_crlf(s);

  dprintf(-dcc[idx].sock, "neg! %s\n", s);
}

void link_done(int idx)
{
  dprintf(-dcc[idx].sock, "neg.\n");
}

int link_find_by_type(int type)
{
  int i = 0;

  for (i = 0; enclink[i].name; i++)
    if (type == enclink[i].type)
      return i;

  return -1;
}

void link_link(int idx, int type, int i, direction_t direction)
{
  if (i == -1 && type != -1) {
    for (i = 0; enclink[i].name; i++) {
      if (enclink[i].type == type)
        break;
    }
  }

  if (i != -1 && enclink[i].link)
    (enclink[i].link) (idx, direction);
  else if (direction == TO)		/* problem finding function, just assume we're done */
    link_done(idx);

  return;
}

int link_read(int snum, char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].read)
    return (enclink[i].read) (snum, buf, len);

  return -1;
}

char *link_write(int snum, char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].write)
    return ((enclink[i].write) (snum, buf, len));

  return buf;
}

void link_hash(int idx, char *rand)
{
  char hash[256] = "";

  /* nothing fancy, just something simple that can stop people from playing */
  simple_snprintf(hash, sizeof(hash), "enclink%s", rand);
  strlcpy(dcc[idx].shahash, SHA1(hash), sizeof(dcc[idx].shahash));
  egg_bzero(hash, sizeof(hash));
  return;
}

void link_parse(int idx, char *buf)
{
  int snum = findanysnum(dcc[idx].sock);
  int i = socklist[snum].enclink;

  if (i >= 0 && enclink[i].parse)
    (enclink[i].parse) (idx, snum, buf);

  return;
}

void link_get_method(int idx)
{
  if (!dcc[idx].type)
    return;

  int n = dcc[idx].u.enc->method_number;

  if (enclink[n].name)
    dcc[idx].u.enc->method = &(enclink[n]);

  ++dcc[idx].u.enc->method_number;
}

/* the order of entries here determines which will be picked */
struct enc_link enclink[] = {
  { "ghost+prand", LINK_GHOSTPRAND, ghost_link_case, ghost_Prand_write, ghost_Prand_read, ghost_Prand_parse },
  { "ghost+case2", LINK_GHOSTCASE2, ghost_link_case, ghost_write, ghost_read, ghost_parse },
// Disabled this one so 1.2.6->1.2.7 will use cleartext, as some 1.2.6 nets have an empty BDHASH
//  { "ghost+case", LINK_GHOSTCASE, ghost_link_case, ghost_write, ghost_read, ghost_parse },
  { "cleartext", LINK_CLEARTEXT, NULL, NULL, NULL, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};
