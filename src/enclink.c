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
#include <bdlib/src/base64.h>

#include <stdarg.h>
#include <signal.h>

#ifdef DEBUG
//#define DEBUG_ENCLINK 1
#endif 
static void ghost_link_case(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (snum >= 0) {
    char initkey[33] = "", *tmp2 = NULL;
    char tmp[70] = "";
    char *keyp = NULL, *nick1 = NULL, *nick2 = NULL;
    port_t port = 0;

    if (direction == TO) {
      keyp = socklist[snum].ikey;
      nick1 = strdup(dcc[idx].nick);
      nick2 = strdup(conf.bot->nick);
      port = htons(dcc[idx].port);
    } else if (direction == FROM) {
      keyp = socklist[snum].okey;
      nick1 = strdup(conf.bot->nick);
      nick2 = strdup(dcc[idx].nick);

      struct sockaddr_in sa;
      socklen_t socklen = sizeof(sa);

      egg_bzero(&sa, socklen);
      getsockname(socklist[snum].sock, (struct sockaddr *) &sa, &socklen);
      port = sa.sin_port;
    }

    /* initkey-gen */
    /* salt1 salt2 port mynick conf.bot->nick */
    sprintf(tmp, STR("%s@%s@%4x@%s@%s"), settings.salt1, settings.salt2, port, strtoupper(nick1), strtoupper(nick2));
    free(nick1);
    free(nick2);
    strlcpy(keyp, SHA1(tmp), ENC_KEY_LEN + 1);
#ifdef DEBUG_ENCLINK
    putlog(LOG_DEBUG, "@", "Link hash for %s: %s", dcc[idx].nick, tmp);
    putlog(LOG_DEBUG, "@", "outkey (%d): %s", strlen(keyp), keyp);
#endif

    OPENSSL_cleanse(tmp, sizeof(tmp));

    if (direction == FROM) {
      make_rand_str(initkey, 32);       /* set the initial out/in link key to random chars. */
      socklist[snum].iseed = socklist[snum].oseed = random();
#ifdef DEBUG_ENCLINK
sdprintf("sock: %d seed: %-10lu %s", snum, socklist[snum].oseed, hexize((unsigned char*) initkey, sizeof(initkey) - 1));
#endif
      tmp2 = encrypt_string(settings.salt2, initkey);
      putlog(LOG_BOTS, "*", STR("Sending encrypted link handshake to %s..."), dcc[idx].nick);

      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;

      link_send(idx, STR("elink %s %lu\n"), tmp2, socklist[snum].oseed);
      free(tmp2);
      strlcpy(socklist[snum].okey, initkey, ENC_KEY_LEN + 1);
      strlcpy(socklist[snum].ikey, initkey, ENC_KEY_LEN + 1);
    } else {
      socklist[snum].encstatus = 1;
      socklist[snum].gz = 1;
    }
  } else {
    putlog(LOG_MISC, "*", STR("Couldn't find socket for %s connection?? Shouldn't happen :/"), dcc[idx].nick);
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

static inline void ghost_cycle_key_in_Prand(int snum) {
  if (socklist[snum].iseed) {
#ifdef DEBUG_ENCLINK
    sdprintf("CYCLING IKEY ON %d", snum);
#endif
    for (size_t i = 0; i < ENC_KEY_LEN; i += sizeof(long))
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
    for (size_t i = 0; i < ENC_KEY_LEN; i += sizeof(long))
      *(long*) &(socklist[snum].okey)[i] = Prand(&(socklist[snum].oseed), 0xFFFFFFFF);

      if (!socklist[snum].oseed)
        ++socklist[snum].oseed;
  }
}

static int ghost_Prand_read(int snum, char *src, size_t *len)
{
  char *b64 = BDLIB_NS::b64dec((unsigned char*) src, len);
  char *line = (char *) decrypt_binary(socklist[snum].ikey, (unsigned char*) b64, len);
  free(b64);

#ifdef DEBUG_ENCLINK
  sdprintf("SEED: %-10lu IKEY: %s", socklist[snum].iseed, hexize((unsigned char*) socklist[snum].ikey, 32));
  sdprintf("READ: %s", line);

  char *p = strchr(line, ' ');
  *(p++) = 0;
  size_t real_len = atoi(line);

  *len -= p - line;

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

  ghost_cycle_key_in_Prand(snum);
  return OK;
}

static char *ghost_Prand_write(int snum, char *src, size_t *len)
{
  char *srcbuf = NULL, *buf = NULL, *line = NULL, *eol = NULL, *eline = NULL;
  unsigned char *edata = NULL;
  size_t bufpos = 0;

  srcbuf = (char *) my_calloc(1, *len + 1);
  strlcpy(srcbuf, src, *len + 1);

  line = srcbuf;

  do {
    eol = strchr(line, '\n');
    *len = eol - line;
    *(eol++) = 0;

    if (*len) {
#ifdef DEBUG_ENCLINK
      char *tmp = (char*) my_calloc(1, *len + 5 + 1);
if (*len != strlen(line)) { sdprintf("WTF 54 %d != %d", *len, strlen(line)); exit(1); }

      /* Add length at beginning to be checked */
      simple_snprintf(tmp, *len + 5 + 1, "%d %s", *len, line);
      char *p = strchr(tmp, ' ');
      *len += ++p - tmp;

      sdprintf("SEED: %-10lu OKEY: %s", socklist[snum].oseed, hexize((unsigned char*) socklist[snum].okey, 32));
      sdprintf("WRITE: %s", tmp);
if (*len != strlen(tmp)) { sdprintf("WTF 1 -- %d != %d", *len, strlen(tmp)); exit(1); }
      edata = encrypt_binary(socklist[snum].okey, (unsigned char*) tmp, len);
      free(tmp);
#else
      edata = encrypt_binary(socklist[snum].okey, (unsigned char*) line, len);
#endif
      eline = BDLIB_NS::b64enc(edata, len);
      free(edata);

      ghost_cycle_key_out_Prand(snum);
 
      buf = (char *) my_realloc(buf, bufpos + *len + 1 + 1);
      strncpy((char *) &buf[bufpos], eline, *len);
      free(eline);
      bufpos += *len;
      buf[bufpos++] = '\n';
    }
    line = eol;
    eol = strchr(line, '\n');
  } while (eol);

  free(srcbuf);

  *len = bufpos;
  buf[*len] = 0;

#ifdef DEBUG_ENCLINK
if (*len != strlen(buf)) { sdprintf("WTF 6 %d != %d", *len, strlen(buf)); exit(1); }
#endif

  return buf;
}

void ghost_Prand_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!egg_strcasecmp(code, STR("elink"))) {
    char *tmp = decrypt_string(settings.salt2, newsplit(&buf));

    socklist[snum].iseed = socklist[snum].oseed = atol(buf);

    /* The hub/sender cycled the oseed once to send us this information.. so cycle/shift our iseed to match */
    /* This operation was only on the seed, not the key, so do it before setting the key */
    ghost_cycle_key_in_Prand(snum);

    strlcpy(socklist[snum].okey, tmp, ENC_KEY_LEN + 1);
    strlcpy(socklist[snum].ikey, tmp, ENC_KEY_LEN + 1);

#ifdef DEBUG_ENCLINK
sdprintf("sock: %d seed: %-10lu %s", snum, socklist[snum].oseed, hexize((unsigned char*) socklist[snum].okey, ENC_KEY_LEN);
#endif
    putlog(LOG_BOTS, "*", STR("Handshake with %s succeeded, we're linked."), dcc[idx].nick);
    free(tmp);
    link_done(idx);
  }
}

void link_send(int idx, const char *format, ...)
{
  char s[2001] = "";
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s) - 1, format, va);
  va_end(va);
  remove_crlf(s);

  dprintf(-dcc[idx].sock, STR("neg! %s\n"), s);
}

void link_done(int idx)
{
  dprintf(-dcc[idx].sock, STR("neg.\n"));
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
  char hash[60] = "";

  /* nothing fancy, just something simple that can stop people from playing */
  simple_snprintf(hash, sizeof(hash), STR("enclink%s"), rand);
  strlcpy(dcc[idx].shahash, SHA1(hash), SHA_HASH_LENGTH + 1);
  OPENSSL_cleanse(hash, sizeof(hash));
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
  { "ghost+prand2", LINK_GHOSTPRAND2, ghost_link_case, ghost_Prand_write, ghost_Prand_read, ghost_Prand_parse },
// Disabled this one so 1.2.6->1.2.7 will use cleartext, as some 1.2.6 nets have an empty BDHASH
//  { "ghost+case", LINK_GHOSTCASE, ghost_link_case, ghost_write, ghost_read, ghost_parse },
  { "cleartext", LINK_CLEARTEXT, NULL, NULL, NULL, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};
