/*
 * Copyright (C) 1997 Robey Pointer
 * Copyright (C) 1999 - 2002 Eggheads Development Team
 * Copyright (C) 2002 - 2014 Bryan Drewery
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* enclink.c
 *
 */


#include "common.h"
#include "enclink.h"
#include "dcc.h"
#include "net.h"
#include "misc.h"

#include <stdarg.h>

static void tls1_link(int idx, direction_t direction)
{
  int snum = findanysnum(dcc[idx].sock);

  if (likely(snum >= 0)) {
    long options = ((direction == TO ? W_SSL_CONNECT : W_SSL_ACCEPT)|W_SSL_VERIFY_PEER|W_SSL_REQUIRE_PEER_CERT);

    // Send SSL handshake
    putlog(LOG_BOTS, "*", STR("Sending SSL handshake to %s..."), dcc[idx].nick);
    if (net_switch_to_ssl(dcc[idx].sock, options, snum) == 0) {
      putlog(LOG_MISC, "*", STR("Failed SSL handshake to %s"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    // Keep as 0 as SSL_read/write is handled specially
    socklist[snum].encstatus = 0;
    dcc[idx].ssl = 1;

    if (options & W_SSL_ACCEPT) {
      link_send(idx, STR("elink !\n"));
    }
  } else {
    putlog(LOG_MISC, "*", STR("Couldn't find socket for %s connection?? Shouldn't happen :/"), dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
}

void tls1_parse(int idx, int snum, char *buf)
{
  /* putlog(LOG_DEBUG, "*", "Got elink: %s %s", code, buf); */
  /* Set the socket key and we're linked */

  char *code = newsplit(&buf);

  if (!strcasecmp(code, STR("elink"))) {
    putlog(LOG_BOTS, "*", STR("Handshake with %s succeeded, we're linked."), dcc[idx].nick);
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

int link_read(int snum, char *buf)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].read)
    return (enclink[i].read) (snum, buf);

  return -1;
}

const char *link_write(int snum, const char *buf, size_t *len)
{
  int i = socklist[snum].enclink;

  if (i != -1 && enclink[i].write)
    return ((enclink[i].write) (snum, buf, len));

  return buf;
}

void link_challenge_to(int idx, char *buf) {
  int snum = findanysnum(dcc[idx].sock);

  if (snum >= 0) {
    char *rand = newsplit(&buf), *tmp = strdup(buf), *tmpp = tmp, *p = NULL;
    int i = -1;

    while ((p = newsplit(&tmp))[0]) {
      if (str_isdigit(p)) {
        int type = atoi(p);

        /* pick the first (lowest num) one that we share */
        i = link_find_by_type(type);

        if (i != -1)
          break;
      }
    }
    free(tmpp);

    // No shared type!
    if (i == -1) {
      sdprintf(STR("No shared cipher with %s"), dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    sdprintf(STR("Choosing '%s' (%d/%d) for link to %s"), enclink[i].name, enclink[i].type, i, dcc[idx].nick);
    link_hash(idx, rand);
    dprintf(-dcc[idx].sock, STR("neg %s %d %s\n"), dcc[idx].shahash, enclink[i].type, dcc[idx].nick);
    socklist[snum].enclink = i;
    link_link(idx, -1, i, TO);
  }
}

void link_hash(int idx, char *rand)
{
  char hash[60] = "";

  /* nothing fancy, just something simple that can stop people from playing */
  simple_snprintf(hash, sizeof(hash), STR("enclink%s"), rand);
  strlcpy(dcc[idx].shahash, SHA1(hash), SHA_HASH_LENGTH + 1);
  SHA1(NULL);
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

  dcc[idx].u.enc->method_number++;
}

/* the order of entries here determines which will be picked */
struct enc_link enclink[] = {
  { "TLS1", LINK_TLS1, tls1_link, NULL, NULL, tls1_parse },
  { "cleartext", LINK_CLEARTEXT, NULL, NULL, NULL, NULL },
  { NULL, 0, NULL, NULL, NULL, NULL }
};
/* vim: set sts=2 sw=2 ts=8 et: */
