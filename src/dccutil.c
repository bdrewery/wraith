/*
 * dccutil.c -- handles:
 *   lots of little functions to send formatted text to
 *   varying types of connections
 *   '.who', '.whom', and '.dccstat' code
 *   memory management for dcc structures
 *   timeout checking for dcc connections
 *
 */


#include <sys/stat.h>
#include "common.h"
#include "color.h"
#include "userrec.h"
#include "dcc.h"
#include "auth.h"
#include "botnet.h"
#include "adns.h"
#include "net.h"
#include "main.h"
#include "dccutil.h"
#include "misc.h"
#include "botcmd.h"
#include <errno.h>
#include "chan.h"
#include "tandem.h"
#include "core_binds.h"
#include "egg_timer.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/notes.mod/notes.h"
#include <stdarg.h>

static struct portmap *root = NULL;

time_t connect_timeout = 40;    /* How long to wait before a telnet connection times out */
int max_dcc = 200;

static int dcc_flood_thr = 3;

void
init_dcc()
{
  if (!conf.bot->hub)
    protect_telnet = 0;
  if (max_dcc < 1)
    max_dcc = 1;
  if (dcc)
    dcc = (struct dcc_t *) my_realloc(dcc, sizeof(struct dcc_t) * max_dcc);
  else
    dcc = (struct dcc_t *) my_calloc(1, sizeof(struct dcc_t) * max_dcc);
}

/* Replace \n with \r\n */
char *
add_cr(char *buf)
{
  static char WBUF[1024] = "";
  char *p = NULL, *q = NULL;

  for (p = buf, q = WBUF; *p; p++, q++) {
    if (*p == '\n')
      *q++ = '\r';
    *q = *p;
  }
  *q = *p;
  return WBUF;
}

static void
colorbuf(char *buf, size_t len, int idx)
{
//  char *buf = *bufp;
  int cidx = coloridx(idx);
  static int cflags;
  int schar = 0;
  char buf3[1024] = "", buf2[1024] = "", c = 0;

  for (size_t i = 0; i < len; i++) {
    c = buf[i];
    buf2[0] = 0;

/*
    if (aqua) {
      if (upper) {
        upper = 0;
        c = toupper(c);
      } else {
        upper = 1;
        c = tolower(c);
      }
    }
*/
    if (cidx) {
      if (schar) {                /* These are for $X replacements */
        schar--;                  /* Unset identifier int */
        switch (c) {
          case 'b':
            if (cflags & CFLGS_BOLD) {
              simple_snprintf(buf2, sizeof(buf2), "%s", BOLD_END(idx));
              cflags &= ~CFLGS_BOLD;
            } else {
              cflags |= CFLGS_BOLD;
              simple_snprintf(buf2, sizeof(buf2), "%s", BOLD(idx));
            }
            break;
          case 'u':
            if (cflags & CFLGS_UNDERLINE) {
              simple_snprintf(buf2, sizeof(buf2), "%s", UNDERLINE_END(idx));
              cflags &= ~CFLGS_UNDERLINE;
            } else {
              simple_snprintf(buf2, sizeof(buf2), "%s", UNDERLINE(idx));
              cflags |= CFLGS_UNDERLINE;
            }
            break;
          case 'f':
            if (cflags & CFLGS_FLASH) {
              simple_snprintf(buf2, sizeof(buf2), "%s", FLASH_END(idx));
              cflags &= ~CFLGS_FLASH;
            } else {
              simple_snprintf(buf2, sizeof(buf2), "%s", FLASH(idx));
              cflags |= CFLGS_FLASH;
            }
            break;
          default:
            simple_snprintf(buf2, sizeof(buf2), "$%c", c);    /* No identifier, put the '$' back in */
            break;
        }
      } else {                    /* These are character replacements */
        switch (c) {
          case '$':
            schar++;
            break;
          case ':':
            simple_snprintf(buf2, sizeof(buf2), "%s%c%s", LIGHTGREY(idx), c, COLOR_END(idx));
            break;
          case '@':
            simple_snprintf(buf2, sizeof(buf2), "%s%c%s", BOLD(idx), c, BOLD_END(idx));
            break;
          case '>':
          case ')':
          case '<':
          case '(':
            simple_snprintf(buf2, sizeof(buf2), "%s%c%s", GREEN(idx), c, COLOR_END(idx));
            break;
          default:
            simple_snprintf(buf2, sizeof(buf2), "%c", c);
            break;
        }
      }
    } else {
      if (schar) {
        schar--;
        switch (c) {
          case 'b':
          case 'u':
          case 'f':
            break;
          default:
            simple_snprintf(buf2, sizeof(buf2), "$%c", c);    /* No identifier, put the '$' back in */
        }
      } else {
        switch (c) {
          case '$':
            schar++;
            break;
          default:
            simple_snprintf(buf2, sizeof(buf2), "%c", c);
            break;
        }
      }
    }
    simple_snprintf(buf3, sizeof(buf3), "%s%s", (buf3 && buf3[0]) ? buf3 : "", (buf2 && buf2[0]) ? buf2 : "");
  }
  buf3[strlen(buf3)] = 0;
  strcpy(buf, buf3);
}

/* Dump a potentially super-long string of text.
 */
void dumplots(int idx, const char *prefix, char *data)
{
  if (!*data) {
    dprintf(idx, "%s\n", prefix);
    return;
  }

  char *p = data, *q = NULL, *n = NULL, c = 0;
  const size_t max_data_len = 120 - strlen(prefix);

  while ((strlen(p) - ansi_len(p)) > max_data_len) {
    q = p + max_data_len;
    /* Search for embedded linefeed first */
    n = strchr(p, '\n');
    if (n && n < q) {
      /* Great! dump that first line then start over */
      *n = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *n = '\n';
      p = n + 1;
    } else {
      /* Search backwards for the last space */
      while (*q != ' ' && q != p)
        q--;
      if (q == p)
        q = p + max_data_len;
      c = *q;
      *q = 0;
      dprintf(idx, "%s%s\n", prefix, p);
      *q = c;
      p = q;
      if (c == ' ')
        p++;
    }
  }
  /* Last trailing bit: split by linefeeds if possible */
  n = strchr(p, '\n');
  while (n) {
    *n = 0;
    dprintf(idx, "%s%s\n", prefix, p);
    *n = '\n';
    p = n + 1;
    n = strchr(p, '\n');
  }
  if (*p)
    dprintf(idx, "%s%s\n", prefix, p);  /* Last trailing bit */
}

void
dprintf(int idx, const char *format, ...)
{
  char buf[1024] = "";
  size_t len;
  va_list va;

  va_start(va, format);
  egg_vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);
  /* We can not use the return value vsnprintf() to determine where
   * to null terminate. The C99 standard specifies that vsnprintf()
   * shall return the number of bytes that would be written if the
   * buffer had been large enough, rather then -1.
   */
  /* We actually can, since if it's < 0 or >= sizeof(buf), we know it wrote
   * sizeof(buf) bytes. But we're not doing that anyway.
   */
  len = strlen(buf);

/* this is for color on dcc :P */

  if (idx < 0) {
    tputs(-idx, buf, len);
  } else if (idx > 0x7FF0) {
    if (idx == DP_STDOUT || idx == DP_STDOUT) {
      colorbuf(buf, len, -1);
      buf[sizeof(buf) - 1] = 0;
      len = strlen(buf);
    }

    switch (idx) {
      case DP_DEBUG:
        sdprintf("%s", buf);
        break;
      case DP_STDOUT:
        tputs(STDOUT, buf, len);
        break;
      case DP_STDERR:
        tputs(STDERR, buf, len);
        break;
      case DP_SERVER:
      case DP_HELP:
      case DP_MODE:
      case DP_MODE_NEXT:
      case DP_SERVER_NEXT:
      case DP_HELP_NEXT:
      case DP_DUMP:
        if (conf.bot->hub)
          break;
        len -= remove_crlf_r(buf);

        if ((idx == DP_DUMP || floodless)) {
         if (serv != -1) {
           if (debug_output)
             putlog(LOG_SRVOUT, "@", "[d->] %s", buf);
           write_to_server(buf, len);
         }
        } else
          queue_server(idx, buf, len);
        break;
    }
    return;
  } else {                      /* normal chat text */
    colorbuf(buf, len, idx);
    buf[sizeof(buf) - 1] = 0;
    len = strlen(buf);

    if (len > 1000) {           /* Truncate to fit */
      buf[1000] = 0;
      strcat(buf, "\n");
      len = 1001;
    }
    if (dcc[idx].simul >= 0 && !dcc[idx].irc) {
      bounce_simul(idx, buf);
    } else if (dcc[idx].irc) {
//      size_t size = strlen(dcc[idx].simulbot) + strlen(buf) + 20;
//      char *ircbuf = (char *) my_calloc(1, size);

//      simple_snprintf(ircbuf, size, "PRIVMSG %s :%s", dcc[idx].simulbot, buf);
//      tputs(dcc[idx].sock, ircbuf, strlen(ircbuf));
      dprintf(DP_HELP, "PRIVMSG %s :%s\n", dcc[idx].simulbot, buf);
//      free(ircbuf);
    } else {
      if (dcc[idx].type && ((long) (dcc[idx].type->output) == 1)) {
        char *p = add_cr(buf);

        tputs(dcc[idx].sock, p, strlen(p));
      } else if (dcc[idx].type && dcc[idx].type->output) {
        dcc[idx].type->output(idx, buf, dcc[idx].u.other);
      } else {
        tputs(dcc[idx].sock, buf, len);
      }
    }
  }
}

void
chatout(const char *format, ...)
{
  char s[1024] = "", *p = NULL;
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s), format, va);
  va_end(va);

  if ((p = strrchr(s, '\n')))
    *p++ = 0;

  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_CHAT) && (dcc[i].simul == -1))
      if (dcc[i].u.chat->channel >= 0)
        dprintf(i, "%s\n", s);
}

/* Print to all on this channel but one.
 */
void
chanout_but(int x, int chan, const char *format, ...)
{
  char s[1024] = "", *p = NULL;
  va_list va;

  va_start(va, format);
  egg_vsnprintf(s, sizeof(s), format, va);
  va_end(va);

  if ((p = strrchr(s, '\n')))
    *p = 0;

  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_CHAT) && (i != x) && (dcc[i].simul == -1))
      if (dcc[i].u.chat->channel == chan)
        dprintf(i, "%s\n", s);
}

void
dcc_chatter(int idx)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);

  if (!glob_party(fr)) {
    dcc[idx].u.chat->channel = -1;
    dprintf(idx, "You don't have partyline chat access; commands only.\n\n");
  } 

  strcpy(dcc[idx].u.chat->con_chan, "***");
  check_chon(dcc[idx].nick, idx);

  dprintf(idx, "Connected to %s, running %s\n", conf.bot->nick, version);
  show_banner(idx);             /* check STAT_BANNER inside function */

  if ((dcc[idx].status & STAT_BOTS) && glob_master(fr)) {
    if ((tands + 1) > 1)
      dprintf(idx, "There are %s-%d- bots%s currently linked.\n", BOLD(idx), tands + 1, BOLD_END(idx));
    else
      dprintf(idx, "There is %s-%d- bot%s currently linked.\n", BOLD(idx), tands + 1, BOLD_END(idx));
    dprintf(idx, " \n");
  }

  if (dcc[idx].status & STAT_WHOM) {
    answer_local_whom(idx, -1);
    dprintf(idx, " \n");
  }

  if (dcc[idx].status & STAT_CHANNELS) {
    show_channels(idx, NULL);
    dprintf(idx, " \n");
  }

  show_motd(idx);

  notes_chon(idx);

  if (dcc[idx].type == &DCC_CHAT) {
    if (!strcmp(dcc[idx].u.chat->con_chan, "***"))
      strcpy(dcc[idx].u.chat->con_chan, "*");

    if (dcc[idx].u.chat->channel == -2)
      dcc[idx].u.chat->channel = 0;

    if (dcc[idx].u.chat->channel >= 0) {
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS) {
        botnet_send_join_idx(idx);
      }
    }

    /* But *do* bother with sending it locally */
    if (!dcc[idx].u.chat->channel) {
      chanout_but(-1, 0, "*** %s joined the party line.\n", dcc[idx].nick);
    } else if (dcc[idx].u.chat->channel > 0) {
      chanout_but(-1, dcc[idx].u.chat->channel, "*** %s joined the channel.\n", dcc[idx].nick);
    }
  }
}

int
dcc_read(FILE *f)
{
  char inbuf[1024] = "", *type = NULL, *buf = NULL;
  int idx = -1;
  bool isserv = 0;

  while (fgets(inbuf, sizeof(inbuf), f) != NULL) {
    remove_crlf(inbuf);
    buf = inbuf;

    if (!strcmp(buf, "+dcc"))
      return idx;
    
    type = newsplit(&buf);
    if (!strcmp(type, "type")) {
      struct dcc_table *dcc_type = NULL;
      size_t dcc_size = 0;

//      if (!strcmp(buf, "CHAT"))
//        dcc_type = &DCC_CHAT;
      if (!strcmp(buf, "SERVER")) {
        dcc_type = &SERVER_SOCKET;
        isserv = 1;
      }
//      if (!strcmp(buf, "BOT"))
//        dcc_type = &DCC_BOT;
    
      if (dcc_type) {
        idx = new_dcc(dcc_type, dcc_size);
        if (isserv)
          servidx = idx;
      }
    }

    if (idx >= 0) {
      if (!strcmp(type, "addr"))
        dcc[idx].addr = my_atoul(buf);
      if (!strcmp(type, "sock")) {
        dcc[idx].sock = atoi(buf);
        if (isserv)
          serv = dcc[idx].sock;
      }
      if (!strcmp(type, "port"))
        dcc[idx].port = atoi(buf);
      if (!strcmp(type, "nick"))
        strlcpy(dcc[idx].nick, buf, NICKLEN);
      if (!strcmp(type, "host")) {
        strlcpy(dcc[idx].host, buf, UHOSTLEN);
      }
    }
  }
  return -1;
}

void 
dcc_write(FILE *f, int idx)
{
  if (dcc[idx].sock > 0) {
    fprintf(f, "-dcc\n");
    if (dcc[idx].type)
      fprintf(f, "type %s\n", dcc[idx].type->name);
//  if (user)
//  fprintf(f, "user %s\n", dcc[idx].user->handle);
    if (dcc[idx].addr)
      fprintf(f, "addr %u\n", dcc[idx].addr);
    if (dcc[idx].status)
      fprintf(f, "status %lu\n", dcc[idx].status);
    fprintf(f, "sock %d\n", dcc[idx].sock);
//  fprintf(f, "simul %d\n", dcc[idx].simul);
    if (dcc[idx].port)
      fprintf(f, "port %d\n", dcc[idx].port);  
    if (dcc[idx].nick[0])
      fprintf(f, "nick %s\n", dcc[idx].nick);
    if (dcc[idx].host[0])
      fprintf(f, "host %s\n", dcc[idx].host);
    fprintf(f, "+dcc\n");
  }
}

/* Mark an entry as lost and deconstruct it's contents. It will be securely
 * removed from the dcc list in the main loop.
 */
void
lostdcc(int n)
{
  sdprintf("lostdcc(%d): %s", n, dcc[n].nick);
  /* Make sure it's a valid dcc index. */
  if (n < 0 || n >= max_dcc)
    return;

  if (dcc[n].type && dcc[n].type->kill)
    dcc[n].type->kill(n, dcc[n].u.other);
  else if (dcc[n].u.other)
    free(dcc[n].u.other);

  dcc[n].u.other = NULL;

//  This is also done when we new_dcc(), so don't bother for now, we set sock/type to NULL, so it won't even be 
//  parsed by anything.
//  egg_bzero(&dcc[n], sizeof(struct dcc_t));

  dcc[n].sock = -1;
  dcc[n].type = NULL;

  dccn--;

  /* last entry! make table smaller :) */
  if (n == (dcc_total - 1))
    dcc_total--;
}

/* Show list of current dcc's to a dcc-chatter
 * positive value: idx given -- negative value: sock given
 */
void
tell_dcc(int idx)
{
  int i;
  size_t j, nicklen = 0;
  char other[160] = "", format[81] = "";

  /* calculate max nicklen */
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type && strlen(dcc[i].nick) > (unsigned) nicklen)
      nicklen = strlen(dcc[i].nick);
  }
  if (nicklen < 9)
    nicklen = 9;

  egg_snprintf(format, sizeof format, "%%-4s %%-4s %%-8s %%-5s %%-%us %%-40s %%s\n", nicklen);
  dprintf(idx, format, "SOCK", "IDX", "ADDR", "PORT", "NICK", "HOST", "TYPE");
  dprintf(idx, format, "----", "---", "--------", "-----", "---------",
          "----------------------------------------", "----");

  egg_snprintf(format, sizeof format, "%%-4d %%-4d %%08X %%5u %%-%us %%-40s %%s\n", nicklen);

  dprintf(idx, "dccn: %d, dcc_total: %d\n", dccn, dcc_total);
  dprintf(idx, "dns_idx: %d, servidx: %d\n", dns_idx, servidx);
  for (i = 0; i < dcc_total; i++) {
    if (dcc[i].type) {
      j = strlen(dcc[i].host);
      if (j > 40)
        j -= 40;
      else
        j = 0;
      if (dcc[i].type && dcc[i].type->display)
        dcc[i].type->display(i, other);
      else {
        sprintf(other, "?:%lX  !! ERROR !!", (long) dcc[i].type);
        break;
      }
      dprintf(idx, format, dcc[i].sock, i, dcc[i].addr, dcc[i].port, dcc[i].nick, dcc[i].host + j, other);
    }
  }
}

/* Mark someone on dcc chat as no longer away
 */
void
not_away(int idx)
{
  if (dcc[idx].u.chat->away == NULL) {
    dprintf(idx, "You weren't away!\n");
    return;
  }
  if (dcc[idx].u.chat->channel >= 0) {
    chanout_but(-1, dcc[idx].u.chat->channel, "*** %s is no longer away.\n", dcc[idx].nick);
    if (dcc[idx].u.chat->channel < GLOBAL_CHANS) {
      botnet_send_away(-1, conf.bot->nick, dcc[idx].sock, NULL, idx);
    }
  }
  dprintf(idx, "You're not away any more.\n");
  free(dcc[idx].u.chat->away);
  dcc[idx].u.chat->away = NULL;
  check_away(conf.bot->nick, idx, NULL);
}

void
set_away(int idx, char *s)
{
  if (s == NULL) {
    not_away(idx);
    return;
  }
  if (!s[0]) {
    not_away(idx);
    return;
  }
  if (dcc[idx].u.chat->away != NULL)
    free(dcc[idx].u.chat->away);
  dcc[idx].u.chat->away = strdup(s);
  if (dcc[idx].u.chat->channel >= 0) {
    chanout_but(-1, dcc[idx].u.chat->channel, "*** %s is now away: %s\n", dcc[idx].nick, s);
    if (dcc[idx].u.chat->channel < GLOBAL_CHANS) {
      botnet_send_away(-1, conf.bot->nick, dcc[idx].sock, s, idx);
    }
  }
  dprintf(idx, "You are now away. (%s)\n", s);
  check_away(conf.bot->nick, idx, s);
}


/* Make a password, 10-14 random letters and digits
 */
void
makepass(char *s)
{
  make_rand_str(s, 10 + randint(5));
}

void
flush_lines(int idx, struct chat_info *ci)
{
  int c = ci->line_count;
  struct msgq *p = ci->buffer, *o;

  while (p && c < (ci->max_line)) {
    ci->current_lines--;
    tputs(dcc[idx].sock, p->msg, p->len);
    free(p->msg);
    o = p->next;
    free(p);
    p = o;
    c++;
  }
  if (p != NULL) {
    if (dcc[idx].status & STAT_TELNET)
      tputs(dcc[idx].sock, "[More]: ", 8);
    else
      tputs(dcc[idx].sock, "[More]\n", 7);
  }
  ci->buffer = p;
  ci->line_count = 0;
}

int
new_dcc(struct dcc_table *type, int xtra_size)
{
  if (!type)
    return -1;

  if (dcc_total == max_dcc)
    return -1;

  int i = 0;

  /* Find the first gap */
  for (i = 0; i <= dcc_total; i++)
    if (!dcc[i].type)
      break;

  /* we managed to get to the end of the list! */
  if (i == dcc_total) {
    i = dcc_total;
    dcc_total++;
  }

  dccn++;

  /* empty out the memory for the entry */
  egg_bzero((char *) &dcc[i], sizeof(struct dcc_t));

  dcc[i].type = type;
  if (xtra_size)
    dcc[i].u.other = (char *) my_calloc(1, xtra_size);
  else
    dcc[i].u.other = NULL;
  dcc[i].simul = -1;
  dcc[i].sock = -1;

  sdprintf("new_dcc (%s): %d (dccn/dcc_total: %d/%d)", type->name, i, dccn, dcc_total);
  return i;
}

/* Changes the given dcc entry to another type.
 */
void
changeover_dcc(int i, struct dcc_table *type, int xtra_size)
{
  /* Free old structure. */
  if (dcc[i].type && dcc[i].type->kill)
    dcc[i].type->kill(i, dcc[i].u.other);
  else if (dcc[i].u.other) {
    free(dcc[i].u.other);
    dcc[i].u.other = NULL;
  }
  dcc[i].type = type;
  if (xtra_size)
    dcc[i].u.other = (char *) my_calloc(1, xtra_size);
}

int
detect_dcc_flood(time_t * timer, struct chat_info *chat, int idx)
{
  if (!dcc_flood_thr)
    return 0;

  time_t t = now;

  if (*timer != t) {
    *timer = t;
    chat->msgs_per_sec = 0;
  } else {
    chat->msgs_per_sec++;
    if (chat->msgs_per_sec > dcc_flood_thr) {
      /* FLOOD */
      dprintf(idx, "*** FLOOD: Goodbye.\n");
      /* Evil assumption here that flags&DCT_CHAT implies chat type */
      if ((dcc[idx].type->flags & DCT_CHAT) && chat && (chat->channel >= 0)) {
        char x[1024];

        simple_snprintf(x, sizeof x, DCC_FLOODBOOT, dcc[idx].nick);
        chanout_but(idx, chat->channel, "*** %s", x);
        if (chat->channel < GLOBAL_CHANS)
          botnet_send_part_idx(idx, x);
      }
      check_chof(dcc[idx].nick, idx);
      if ((dcc[idx].sock != STDOUT) || backgrd) {
        killsock(dcc[idx].sock);
        lostdcc(idx);
      } else {
        dprintf(DP_STDOUT, "\n### SIMULATION RESET ###\n\n");
        dcc_chatter(idx);
      }
      return 1;                 /* <- flood */
    }
  }
  return 0;
}

/* Handle someone being booted from dcc chat.
 */
void
do_boot(int idx, char *by, char *reason)
{
  dprintf(idx, DCC_BOOTED1);
  dprintf(idx, DCC_BOOTED2, by, reason[0] ? ": " : ".", reason);
  /* If it's a partyliner (chatterer :) */
  /* Horrible assumption that DCT_CHAT using structure uses same format
   * as DCC_CHAT */
  if ((dcc[idx].type->flags & DCT_CHAT) && (dcc[idx].u.chat->channel >= 0)) {
    char x[1024] = "";

    simple_snprintf(x, sizeof x, DCC_BOOTED3, by, dcc[idx].nick, reason[0] ? ": " : "", reason);
    chanout_but(idx, dcc[idx].u.chat->channel, "*** %s.\n", x);
    if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
      botnet_send_part_idx(idx, x);
  }
  check_chof(dcc[idx].nick, idx);

  if (dcc[idx].u.chat->su_nick) {
    dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->su_nick);
    strcpy(dcc[idx].nick, dcc[idx].u.chat->su_nick);
    dcc[idx].type = &DCC_CHAT;
    dprintf(idx, "Returning to real nick %s!\n", dcc[idx].u.chat->su_nick);
    free(dcc[idx].u.chat->su_nick);
    dcc[idx].u.chat->su_nick = NULL;
    dcc_chatter(idx);

    if (dcc[idx].u.chat->channel < GLOBAL_CHANS && dcc[idx].u.chat->channel >= 0) {
      botnet_send_join_idx(idx);
    }
  } else if ((dcc[idx].sock != STDOUT) || backgrd) {
    killsock(dcc[idx].sock);
    lostdcc(idx);
    /* Entry must remain in the table so it can be logged by the caller */
  } else {
    dprintf(DP_STDOUT, "\n### SIMULATION RESET\n\n");
    dcc_chatter(idx);
  }
  return;
}

int
listen_all(port_t lport, bool off)
{
  int idx = -1;
  port_t port, realport;

#ifdef USE_IPV6
  int i6 = 0;
#endif /* USE_IPV6 */
  int i = 0, ii = 0;
  struct portmap *pmap = NULL, *pold = NULL;

  port = realport = lport;
  for (pmap = root; pmap; pold = pmap, pmap = pmap->next)
    if (pmap->realport == port) {
      port = pmap->mappedto;
      break;
    }

  for (ii = 0; ii < dcc_total; ii++) {
    if (dcc[ii].type && (dcc[ii].type == &DCC_TELNET) && (dcc[ii].port == port)) {
      idx = ii;

      if (off) {
        if (pmap) {
          if (pold)
            pold->next = pmap->next;
          else
            root = pmap->next;
          free(pmap);
        }
#ifdef USE_IPV6
        if (sockprotocol(dcc[idx].sock) == AF_INET6)
          putlog(LOG_DEBUG, "*", "Closing IPv6 listening port %d", dcc[idx].port);
        else
#endif /* USE_IPV6 */
          putlog(LOG_DEBUG, "*", "Closing IPv4 listening port %d", dcc[idx].port);
        killsock(dcc[idx].sock);
        lostdcc(idx);
        return idx;
      }
    }
  }
  if (idx < 0) {
    if (off) {
      putlog(LOG_ERRORS, "*", "No such listening port open - %d", lport);
      return idx;
    }
    /* make new one */
    if (dcc_total >= max_dcc) {
      putlog(LOG_ERRORS, "*", "Can't open listening port - no more DCC Slots");
    } else {
#ifdef USE_IPV6
      i6 = open_listen_by_af(&port, AF_INET6);
      if (i6 < 0) {
        putlog(LOG_ERRORS, "*", "Can't open IPv6 listening port %d - %s", port,
               i6 == -1 ? "it's taken." : "couldn't assign ip.");
      } else {
        /* now setup ipv4/ipv6 listening port */
        idx = new_dcc(&DCC_TELNET, 0);
        dcc[idx].addr = 0L;
        strcpy(dcc[idx].host6, myipstr(6));
        dcc[idx].port = port;
        dcc[idx].sock = i6;
        dcc[idx].timeval = now;
        strcpy(dcc[idx].nick, "(telnet6)");
        strcpy(dcc[idx].host, "*");
        putlog(LOG_DEBUG, "*", "Listening on IPv6 at telnet port %d", port);
      }
      i = open_listen_by_af(&port, AF_INET);
#else
      i = open_listen(&port);
#endif /* USE_IPV6 */
      if (i < 0) {
#ifdef USE_IPV6
        if (i6 < 0)
#endif /* USE_IPV6 */
          putlog(LOG_ERRORS, "*", "Can't open IPv4 listening port %d - %s", port,
                 i == -1 ? "it's taken." : "couldn't assign ip.");
      } else {
        /* now setup ipv4 listening port */
        idx = new_dcc(&DCC_TELNET, 0);
        dcc[idx].addr = iptolong(getmyip());
        dcc[idx].port = port;
        dcc[idx].sock = i;
        dcc[idx].timeval = now;
        strcpy(dcc[idx].nick, "(telnet)");
        strcpy(dcc[idx].host, "*");
        putlog(LOG_DEBUG, "*", "Listening on IPv4 at telnet port %d", port);
      }
#ifdef USE_IPV6
      if (i > 0 || i6 > 0) {
#else
      if (i > 0) {
#endif /* USE_IPV6 */
        if (!pmap) {
          pmap = (struct portmap *) my_calloc(1, sizeof(struct portmap));
          pmap->next = root;
          root = pmap;
        }
        pmap->realport = realport;
        pmap->mappedto = port;
      }
    }
  }
  /* if one of the protocols failed, the one which worked will be returned
   * if both were successful, it wont matter which idx is returned, because the 
   * code reading listen_all will only be reading dcc[idx].port, which would be
   * open on both protocols.
   * -bryan (10/29/03)
   */
  return idx;
}

void
identd_open()
{
  int idx;
  int i = -1;
  port_t port = 113;

  for (idx = 0; idx < dcc_total; idx++)
    if (dcc[idx].type == &DCC_IDENTD_CONNECT)
      return;                   /* it's already open :) */

  idx = -1;

  identd_hack = 1;
#ifdef USE_IPV6
  i = open_listen_by_af(&port, AF_INET6);
#else
  i = open_listen(&port);
#endif /* USE_IPV6 */
  identd_hack = 0;
  if (i >= 0) {
    idx = new_dcc(&DCC_IDENTD_CONNECT, 0);
    if (idx >= 0) {
      egg_timeval_t howlong;

      dcc[idx].addr = iptolong(getmyip());
      dcc[idx].port = port;
      dcc[idx].sock = i;
      dcc[idx].timeval = now;
      strcpy(dcc[idx].nick, "(identd)");
      strcpy(dcc[idx].host, "*");
      putlog(LOG_DEBUG, "*", "Identd daemon started.");
      howlong.sec = 15;
      howlong.usec = 0;
      timer_create(&howlong, "identd_close()", (Function) identd_close);
    } else
      killsock(i);
  }
}

void
identd_close()
{
  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type == &DCC_IDENTD_CONNECT) {
      killsock(dcc[idx].sock);
      lostdcc(idx);
      putlog(LOG_DEBUG, "*", "Identd daemon stopped.");
      break;
    }
  }
}

bool
valid_idx(int idx)
{
  if ((idx == -1) || (idx >= dcc_total) || (!dcc[idx].type))
    return 0;
  return 1;
}

int check_cmd_pass(const char *cmd, char *pass)
{
  if (check_master_hash(NULL, pass))
    return 1;

  struct cmd_pass *cp = NULL;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!egg_strcasecmp(cmd, cp->name)) {
      char tmp[32] = "";

      encrypt_cmd_pass(pass, tmp);
      if (!strcmp(tmp, cp->pass))
        return 1;
      return 0;
    }
  return 0;
}

int has_cmd_pass(const char *cmd)
{
  struct cmd_pass *cp = NULL;

  for (cp = cmdpass; cp; cp = cp->next)
    if (!egg_strcasecmp(cmd, cp->name))
      return 1;
  return 0;
}

void set_cmd_pass(char *ln, int shareit)
{
  struct cmd_pass *cp = NULL;
  char *cmd = NULL;

  cmd = newsplit(&ln);
  for (cp = cmdpass; cp; cp = cp->next)
    if (!strcmp(cmd, cp->name))
      break;
  if (cp)
    if (ln[0]) {
      /* change */
      strcpy(cp->pass, ln);
      if (shareit)
        botnet_send_cmdpass(-1, cp->name, cp->pass);
    } else {
      if (cp == cmdpass)
        cmdpass = cp->next;
      else {
        struct cmd_pass *cp2;

        cp2 = cmdpass;
        while (cp2->next != cp)
          cp2 = cp2->next;
        cp2->next = cp->next;
      }
      if (shareit)
        botnet_send_cmdpass(-1, cp->name, "");
      free(cp->name);
      free(cp);
  } else if (ln[0]) {
    /* create */
    cp = (struct cmd_pass *) my_calloc(1, sizeof(struct cmd_pass));
    cp->next = cmdpass;
    cmdpass = cp;
    cp->name = strdup(cmd);
    strcpy(cp->pass, ln);
    if (shareit)
      botnet_send_cmdpass(-1, cp->name, cp->pass);
  }
}

void cmdpass_free(struct cmd_pass *x) 
{
  struct cmd_pass *cp = NULL, *cp_n = NULL;

  for (cp = x; cp; cp = cp_n) {
    cp_n = cp->next;
    list_delete((struct list_type **) &x, (struct list_type *) cp);
    free(cp->name);
    free(cp);
  }
}
