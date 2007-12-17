/*
 * dcc.c -- handles:
 *   activity on a dcc socket
 *   disconnect on a dcc socket
 *   ...and that's it!  (but it's a LOT)
 *
 */


#include "common.h"
#include "dcc.h"
#include "settings.h"
#include "enclink.h"
#include "binds.h"
#include "adns.h"
#include "main.h"
#include "cmds.h"
#include "color.h"
#include "net.h"
#include "response.h"
#include "misc.h"
#include "users.h"
#include "userrec.h"
#include "userent.h"
#include "match.h"
#include "auth.h"
#include "dccutil.h"
#include "crypt.h"
#include "chanprog.h"
#include "botmsg.h"
#include "botcmd.h"
#include "botnet.h"
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include "tandem.h"
#include "core_binds.h"


struct cmd_pass *cmdpass = NULL;
struct dcc_t *dcc = NULL;       /* DCC list                                */
time_t timesync = 0;
int dcc_total = 0;              /* size of dcc table                             */
int dccn = 0;			/* actual number of dcc entries */
int uplink_idx = -1;

static time_t password_timeout = 40;       /* Time to wait for a password from a user */
static time_t auth_timeout = 80;
static time_t bot_timeout = 15;    /* Bot timeout value                       */
static time_t identtimeout = 15;   /* Timeout value for ident lookups         */
static time_t dupwait_timeout = 5; /* Timeout for rejecting duplicate entries */

bool protect_telnet = 0;  /* Even bother with ident lookups :)       */
static int flood_telnet_thr = 10;       /* Number of telnet connections to be
                                         * considered a flood                      */
static time_t flood_telnet_time = 5;       /* In how many seconds?                    */

static void dcc_telnet_got_ident(int, char *);
static void dcc_telnet_pass(int, int);

static void
strip_telnet(int sock, char *buf, int *len)
{
  unsigned char *p = (unsigned char *) buf, *o = (unsigned char *) buf;
  int mark;

  while (*p != 0) {
    while ((*p != TLN_IAC) && (*p != 0)) {
      if (*p == 0xA0) {
        *o++ = 32;
        p++;
      } else
        *o++ = *p++;
    }

    if (*p == TLN_IAC) {
      p++;
      mark = 2;
      if (!*p)
        mark = 1;               /* bogus */
      if ((*p >= TLN_WILL) && (*p <= TLN_DONT)) {
        mark = 3;
        if (!*(p + 1))
          mark = 2;             /* bogus */
      }
      if (*p == TLN_WILL) {
        /* WILL X -> response: DONT X */
        /* except WILL ECHO which we just smile and ignore */
        if (*(p + 1) != TLN_ECHO) {
          write(sock, TLN_IAC_C TLN_DONT_C, 2);
          write(sock, p + 1, 1);
        }
      }
      if (*p == TLN_DO) {
        /* DO X -> response: WONT X */
        /* except DO ECHO which we just smile and ignore */
        if (*(p + 1) != TLN_ECHO) {
          write(sock, TLN_IAC_C TLN_WONT_C, 2);
          write(sock, p + 1, 1);
        }
      }
      if (*p == TLN_AYT) {
        /* "are you there?" */
        /* response is: "hell yes!" */
        write(sock, "\r\nHell, yes!\r\n", 14);
      }
      /* Anything else can probably be ignored */
      p += mark - 1;
      *len = *len - mark;
    }
  }
  *o = *p;
}

void
send_sysinfo()
{
  char *username = NULL, *sysname = NULL, *nodename = NULL, *arch = NULL, *osver = NULL;
  struct utsname un;
  bool gotun = 0;

  if (uname(&un) < 0)
    gotun = 0;
  else 
    gotun = 1;

  username = (char *) get_user(&USERENTRY_USERNAME, conf.bot->u);
  sysname = (char *) get_user(&USERENTRY_OS, conf.bot->u);
  nodename = (char *) get_user(&USERENTRY_NODENAME, conf.bot->u);
  arch = (char *) get_user(&USERENTRY_ARCH, conf.bot->u);
  osver = (char *) get_user(&USERENTRY_OSVER, conf.bot->u);

  const char *usysname = NULL, *uusername = NULL, *unodename = NULL, *uarch = NULL, *uosver = NULL;

  usysname = gotun ? un.sysname : "*";
  uusername = conf.username ? conf.username : "*";
  unodename = gotun ? un.nodename : "*";
  uarch = gotun ? un.machine : "*";
  uosver = gotun ? un.release : "*";

  if (((sysname && egg_strcasecmp(sysname, usysname)) ||
       (username && egg_strcasecmp(username, uusername)) ||
       (nodename && egg_strcasecmp(nodename, unodename)) ||
       (arch && egg_strcasecmp(arch, uarch)) ||
       (osver && egg_strcasecmp(osver, uosver))
      ) ||
      ((!sysname && usysname) || 
       (!username && uusername) || 
       (!nodename && unodename) || 
       (!arch && uarch) ||
       (!osver && uosver)
      )
      ) {
      char buf[201] = "";
      size_t len = 0;

      len = simple_snprintf(buf, sizeof(buf), "si %s %s %s %s %s\n",
            conf.username ? conf.username : "*", gotun ? un.sysname : "*", gotun ? un.nodename : "*",
            gotun ? un.machine : "*", gotun ? un.release : "*");

      send_uplink(buf, len);
  }
}

void
send_timesync(int idx)
{
  /* Send timesync to idx, or all lower bots if idx<0 */
  if (idx >= 0)
    dprintf(idx, "ts %li\n", timesync + now);
  else {
    char s[15] = "";

    sprintf(s, "ts %li\n", timesync + now);
    for (int i = 0; i < dcc_total; i++) {
      if (dcc[i].type && (dcc[i].type == &DCC_BOT) && (bot_aggressive_to(dcc[i].user))) {
        dprintf(i, s);
      }
    }
  }
}

static void
greet_new_bot(int idx)
{
  dcc[idx].timeval = now;
  dcc[idx].u.bot->version[0] = 0;
  dcc[idx].u.bot->sysname[0] = 0;
  dcc[idx].u.bot->numver = 0;
  if (conf.bot->hub && dcc[idx].user && (!(dcc[idx].user->flags & USER_OP))) {
    putlog(LOG_BOTS, "*", "Rejecting link from %s", dcc[idx].nick);
    dprintf(idx, "error You are being rejected.\n");
    dprintf(idx, "bye\n");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if (bot_hublevel(dcc[idx].user) == 999)
    dcc[idx].status |= STAT_LEAF;
  dcc[idx].status |= STAT_LINKING;

  dprintf(idx, "v 1001500 %d Wraith %s <%s> %d %li %s\n", HANDLEN, egg_version, "-", conf.bot->localhub, buildts, egg_version);

  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && dcc[i].type == &DCC_FORK_BOT) {
      killsock(dcc[i].sock);
      lostdcc(i);
    }
  }
}

static void
bot_version(int idx, char *par)
{
  dcc[idx].timeval = now;
  if (in_chain(dcc[idx].nick)) {
    dprintf(idx, "error Sorry, already connected.\n");
    dprintf(idx, "bye\n");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if ((par[0] >= '0') && (par[0] <= '9')) {
    char *work = NULL;

    work = newsplit(&par);
    dcc[idx].u.bot->numver = atoi(work);
    /* old numver crap */
  } else
    dcc[idx].u.bot->numver = 0;

  dprintf(idx, "tb %s\n", conf.bot->nick);

  size_t l = atol(newsplit(&par));

  if (l != HANDLEN) {
    putlog(LOG_BOTS, "*", "Non-matching handle lengths with %s, they use %d characters.", dcc[idx].nick, l);
    dprintf(idx, "error Non-matching handle length: mine %d, yours %d\n", HANDLEN, l);
    dprintf(idx, "bye %s\n", "bad handlen");
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  char x[1024] = "", *vversion = NULL;
  int vlocalhub = 0;
  time_t vbuildts = 0;

  strlcpy(dcc[idx].u.bot->version, par, 120);
  newsplit(&par);               /* 'ver' */
  newsplit(&par);               /* handlen */
  newsplit(&par);               /* network */
  if (par[0])
    vlocalhub = atoi(newsplit(&par));
  if (par[0])
    vbuildts = atol(newsplit(&par));
  if (par[0])
    vversion = newsplit(&par);

  if (conf.bot->hub) {
    putlog(LOG_BOTS, "*", "Linked to %s.\n", dcc[idx].nick);
    chatout("*** Linked to %s.\n", dcc[idx].nick);

    if (bot_hublevel(dcc[idx].user) < 999) {
      if (!bot_aggressive_to(dcc[idx].user))    //not aggressive, so they are technically my uplink.
        uplink_idx = idx;
      dcc[idx].hub = 1;
    }

    botnet_send_nlinked(idx, dcc[idx].nick, conf.bot->nick, '!', vlocalhub, vbuildts, vversion);
  } else {
    putlog(LOG_BOTS, "*", "Linked to botnet.");
    chatout("*** Linked to botnet.\n");
    uplink_idx = idx;
    dcc[idx].hub = 1;
  }

  dump_links(idx);

  touch_laston(dcc[idx].user, "linked", now);
  dcc[idx].type = &DCC_BOT;
  addbot(dcc[idx].nick, dcc[idx].nick, conf.bot->nick, '-', vlocalhub, vbuildts, vversion);
  simple_snprintf(x, sizeof x, "v 1001500");
  bot_shareupdate(idx, x);
  bot_share(idx, x);
  dprintf(idx, "el\n");
}

void
failed_link(int idx)
{
  char s[NICKLEN] = "", s1[512] = "";

  if (dcc[idx].u.bot->linker[0]) {
    simple_snprintf(s, sizeof s, "Couldn't link to %s.", dcc[idx].nick);
    strcpy(s1, dcc[idx].u.bot->linker);
    add_note(s1, conf.bot->nick, s, -2, 0);
  }
  if (dcc[idx].u.bot->numver >= (-1))
    putlog(LOG_BOTS, "*", DCC_LINKFAIL, dcc[idx].nick);
  if (dcc[idx].sock != -1) {
    killsock(dcc[idx].sock);
    dcc[idx].sock = -1;
  }
  strlcpy(s, dcc[idx].nick, sizeof(s));
  lostdcc(idx);
  autolink_cycle(s);            /* Check for more auto-connections */
}

static void
cont_link(int idx, char *buf, int ii)
{
  /* If we're already connected somewhere, unlink and idle a sec */
  for (int i = 0; i < dcc_total; i++) {
    if (dcc[i].type && (dcc[i].type == &DCC_BOT) && (!bot_aggressive_to(dcc[i].user))) {
      putlog(LOG_BOTS, "*", "Unlinking %s - restructure", dcc[i].nick);
      botnet_send_unlinked(i, dcc[i].nick, "Restructure");
      killsock(dcc[i].sock);
      lostdcc(i);
      usleep(1000 * 500);
      break;
    }
  }

/*.    ssl_link(dcc[idx].sock, CONNECT_SSL); */

  dcc[idx].type = &DCC_BOT_NEW;
  dcc[idx].u.bot->numver = 0;

  if (ii == 3)
    dprintf(idx, "-%s\n", conf.bot->nick);
    /* wait for "neg?" now */

  /* now we wait to negotiate an encryption */
  return;
}

static void
dcc_bot_new(int idx, char *buf, int x)
{
/*  struct userrec *u = get_user_by_handle(userlist, dcc[idx].nick); */
  char *code = NULL;

  strip_telnet(dcc[idx].sock, buf, &x);
  code = newsplit(&buf);
  if (!egg_strcasecmp(code, "goodbye!")) {
    greet_new_bot(idx);
  } else if (!egg_strcasecmp(code, "v")) {
    bot_version(idx, buf);
  } else if (!egg_strcasecmp(code, "neg!")) {	/* something to parse in enclink.c */
    link_parse(idx, buf);
  } else if (!egg_strcasecmp(code, "neg?")) {	/* we're connecting to THEM */
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

      sdprintf("Choosing '%s' (%d/%d) for link", enclink[i].name, enclink[i].type, i);
      link_hash(idx, rand);
      dprintf(-dcc[idx].sock, "neg %s %d\n", dcc[idx].shahash, enclink[i].type);
      socklist[snum].enclink = i;
      link_link(idx, -1, i, TO);
    }
  } else if (!egg_strcasecmp(code, "error")) {
    putlog(LOG_MISC, "*", "ERROR linking %s: %s", dcc[idx].nick, buf);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  } else if (strcmp(code, "")) {
    /* Invalid password/digest on leaf */
    putlog(LOG_WARN, "*", "%s failed encrypted link handshake", dcc[idx].nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  }
  /* Ignore otherwise */
}

static void
eof_dcc_bot_new(int idx)
{
  putlog(LOG_BOTS, "*", DCC_LOSTBOT, dcc[idx].nick);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
timeout_dcc_bot_new(int idx)
{
  if (conf.bot->hub)
    putlog(LOG_BOTS, "*", DCC_TIMEOUT, dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  else
    putlog(LOG_BOTS, "*", "Timeout: bot link to %s", dcc[idx].nick);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_bot_new(int idx, char *buf)
{
  sprintf(buf, "bot*  waited %lis", now - dcc[idx].timeval);
}

static void
free_dcc_bot_(int n, void *x)
{
  if (dcc[n].type == &DCC_BOT) {
    unvia(n, findbot(dcc[n].nick));
    rembot(dcc[n].nick);
  }
  free(x);
}

struct dcc_table DCC_BOT_NEW = {
  "BOT_NEW",
  0,
  eof_dcc_bot_new,
  dcc_bot_new,
  &bot_timeout,
  timeout_dcc_bot_new,
  display_dcc_bot_new,
  free_dcc_bot_,
  NULL,
  NULL
};

/* Hash function for tandem bot commands */
extern botcmd_t C_bot[];

static void
dcc_bot(int idx, char *code, int i)
{
  char *msg = NULL;

  strip_telnet(dcc[idx].sock, code, &i);
  if (debug_output) {
/*    if (code[0] != 'z' && code[1] != 'b' && code[2] != ' ') { */
    if (code[0] == 's')
      putlog(LOG_BOTSHARE, "@", "{%s} %s", dcc[idx].nick, code);
    else
      putlog(LOG_BOTNET, "@", "<-[%s] %s", dcc[idx].nick, code);
/*     } */
  }
  msg = strchr(code, ' ');
  if (msg) {
    *msg = 0;
    msg++;
  } else
    msg = "";

  for (i = 0; C_bot[i].name; i++) {
    int y = egg_strcasecmp(code, C_bot[i].name);

    if (!y) {
      /* Found a match */
      if (have_cmd(NULL, C_bot[i].type))
        (C_bot[i].func) (idx, msg);
      break;
    } else if (y < 0)
      break;
  }
}

static void
eof_dcc_bot(int idx)
{
  char x[1024] = "";
  int bots, users;

  bots = bots_in_subtree(findbot(dcc[idx].nick));
  users = users_in_subtree(findbot(dcc[idx].nick));
  simple_snprintf(x, sizeof x,
               "Lost bot: %s (lost %d bot%s and %d user%s)",
               dcc[idx].nick, bots, (bots != 1) ? "s" : "", users, (users != 1) ? "s" : "");
  putlog(LOG_BOTS, "*", "%s.", x);
  chatout("*** %s\n", x);

  botnet_send_unlinked(idx, dcc[idx].nick, x);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_bot(int idx, char *buf)
{
  size_t i = simple_sprintf(buf, "bot   flags: ");

  buf[i++] = b_status(idx) & STAT_PINGED ? 'P' : 'p';
  buf[i++] = b_status(idx) & STAT_SHARE ? 'U' : 'u';
  buf[i++] = b_status(idx) & STAT_CALLED ? 'C' : 'c';
  buf[i++] = b_status(idx) & STAT_OFFERED ? 'O' : 'o';
  buf[i++] = b_status(idx) & STAT_SENDING ? 'S' : 's';
  buf[i++] = b_status(idx) & STAT_GETTING ? 'G' : 'g';
  buf[i++] = b_status(idx) & STAT_WARNED ? 'W' : 'w';
  buf[i++] = b_status(idx) & STAT_LEAF ? 'L' : 'l';
  buf[i++] = b_status(idx) & STAT_LINKING ? 'I' : 'i';
  buf[i++] = b_status(idx) & STAT_AGGRESSIVE ? 'a' : 'A';
  buf[i++] = b_status(idx) & STAT_OFFEREDU ? 'B' : 'b';
  buf[i++] = b_status(idx) & STAT_SENDINGU ? 'D' : 'd';
  buf[i++] = b_status(idx) & STAT_GETTINGU ? 'E' : 'e';
#ifdef USE_IPV6
  if (sockprotocol(dcc[idx].sock) == AF_INET6 && dcc[idx].host6[0])
    buf[i++] = '6';
#endif /* USE_IPV6 */
  buf[i++] = 0;
}

static void
display_dcc_fork_bot(int idx, char *buf)
{
  simple_sprintf(buf, "conn  bot");
}

struct dcc_table DCC_BOT = {
  "BOT",
  DCT_BOT,
  eof_dcc_bot,
  dcc_bot,
  NULL,
  NULL,
  display_dcc_bot,
  free_dcc_bot_,
  NULL,
  NULL
};

struct dcc_table DCC_FORK_BOT = {
  "FORK_BOT",
  0,
  failed_link,
  cont_link,
  &connect_timeout,
  failed_link,
  display_dcc_fork_bot,
  free_dcc_bot_,
  NULL,
  NULL
};

static void
dcc_identd(int idx, char *buf, int atr)
{
  char outbuf[1024] = "";

  size_t len = simple_snprintf(outbuf, sizeof outbuf, "%s : USERID : UNIX : %s\n", buf, botuser);
  tputs(dcc[idx].sock, outbuf, len);

  /* just close it, functions neededing it will open it. */
  identd_close();
}

static void
eof_dcc_identd(int idx)
{
  /* dont bother logging it, who gives a fuck */
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_identd(int idx, char *buf)
{
  simple_sprintf(buf, "idtd  %d%s", dcc[idx].port, (dcc[idx].status & LSTN_PUBLIC) ? " pub" : "");
}

struct dcc_table DCC_IDENTD = {
  "IDENTD",
  DCT_LISTEN,
  eof_dcc_identd,
  dcc_identd,
  NULL,
  NULL,
  display_dcc_identd,
  NULL,
  NULL,
  NULL
};

static void
dcc_identd_connect(int idx, char *buf, int atr)
{
  in_addr_t ip;
  port_t port;
  int j, sock;
  char s[UHOSTLEN + 1] = "";

  if (dcc_total + 1 > max_dcc) {
    j = answer(dcc[idx].sock, s, &ip, &port, 0);
    if (j != -1)
      killsock(j);
    return;
  }
  sock = answer(dcc[idx].sock, s, &ip, &port, 0);

  while ((sock == -1) && (errno == EAGAIN))
    sock = answer(sock, s, &ip, &port, 0);

  if (sock < 0) {
    putlog(LOG_MISC, "*", DCC_FAILED, strerror(errno));
    return;
  }
  /* changeover_dcc(idx, &DCC_IDENTD, 0); */

  j = new_dcc(&DCC_IDENTD, 0);

  dcc[j].sock = sock;
  dcc[j].port = port;
  dcc[j].addr = dcc[idx].addr;
  strcpy(dcc[j].host, dcc[idx].host);
  strcpy(dcc[j].nick, "*");
  /* dcc[j].u.ident_sock = dcc[idx].sock; */
  dcc[j].timeval = now;
}

struct dcc_table DCC_IDENTD_CONNECT = {
  "IDENTD",
  DCT_LISTEN,
  eof_dcc_identd,
  dcc_identd_connect,
  NULL,
  NULL,
  display_dcc_identd,
  NULL,
  NULL,
  NULL
};

static void
dcc_chat_secpass(int idx, char *buf, int atr)
{
  int badauth = 0;

  if (!atr)
    return;

  strip_telnet(dcc[idx].sock, buf, &atr);
  atr = dcc[idx].user ? dcc[idx].user->flags : 0;

  if (dccauth) {
    char check[MD5_HASH_LENGTH + 7] = "";

    simple_snprintf(check, sizeof check, "+Auth %s", dcc[idx].hash);
    badauth = strcmp(check, buf);
    /* +secpass */
  }

  /* Correct pass or secpass! */
  if (!dcc[idx].wrong_pass && (!dccauth || (dccauth && !badauth))) {
    putlog(LOG_MISC, "*", DCC_LOGGEDIN, dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {
      free(dcc[idx].u.chat->away);
      dcc[idx].u.chat->away = NULL;
    }
    dcc[idx].type = &DCC_CHAT;
    dcc[idx].status &= ~STAT_CHAT;
    dcc[idx].u.chat->channel = -2;
    /* Turn echo back on for telnet sessions (send IAC WON'T ECHO). */
    if (dcc[idx].status & STAT_TELNET)
      dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
    stats_add(dcc[idx].user, 1, 0);
    if (!get_user(&USERENTRY_SECPASS, dcc[idx].user)) {	/* this should check how many logins instead */
      char pass[MAXPASSLEN + 1] = "";

      dprintf(idx, "********************************************************************\n \n \n");
      dprintf(idx, "%sWARNING: YOU DO NOT HAVE A SECPASS SET, NOW SETTING A RANDOM ONE....%s\n", FLASH(-1), FLASH_END(-1));
      make_rand_str(pass, MAXPASSLEN);
      set_user(&USERENTRY_SECPASS, dcc[idx].user, pass);
      if (conf.bot->hub)
        write_userfile(idx);
      dprintf(idx, "Your secpass is now: %s%s%s\n", pass, BOLD(-1), BOLD_END(-1));
      dprintf(idx, "Make sure you do not lose this, as it is needed to login for now on.\n \n");
      dprintf(idx, "********************************************************************\n");
    }
    dcc_chatter(idx);
  } else if ((dccauth && badauth) || dcc[idx].wrong_pass) { 		/* bad auth */
    dprintf(idx, "%s\n", response(RES_BADUSERPASS));
    putlog(LOG_MISC, "*", DCC_BADAUTH, dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {        /* su from a dumb user */
      /* Turn echo back on for telnet sessions (send IAC WON'T ECHO). */
      if (dcc[idx].status & STAT_TELNET)
        dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
      dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->away);
      strcpy(dcc[idx].nick, dcc[idx].u.chat->away);
      free(dcc[idx].u.chat->away);
      free(dcc[idx].u.chat->su_nick);
      dcc[idx].u.chat->away = NULL;
      dcc[idx].u.chat->su_nick = NULL;
      dcc[idx].type = &DCC_CHAT;

      dcc[idx].u.chat->channel = dcc[idx].u.chat->su_channel;
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
        botnet_send_join_idx(idx);
      chanout_but(-1, dcc[idx].u.chat->channel, DCC_JOIN, dcc[idx].nick);
    } else {
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  }
}

static void
eof_dcc_general(int idx)
{
  putlog(LOG_MISC, "*", DCC_LOSTDCC, dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
tout_dcc_chat_secpass(int idx)
{
  putlog(LOG_MISC, "*", DCC_SPWDTIMEOUT, dcc[idx].nick, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_chat_secpass(int idx, char *buf)
{
  sprintf(buf, "secpass  waited %lis", now - dcc[idx].timeval);
}

static void
tout_dcc_chat_pass(int idx)
{
  putlog(LOG_MISC, "*", DCC_PWDTIMEOUT, dcc[idx].nick, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_chat_pass(int idx, char *buf)
{
  sprintf(buf, "pass  waited %lis", now - dcc[idx].timeval);
}

static void
kill_dcc_general(int idx, void *x)
{
  register struct chat_info *p = (struct chat_info *) x;

  if (p) {
    if (p->buffer) {
      struct msgq *r = NULL, *q = NULL;

      for (r = dcc[idx].u.chat->buffer; r; r = q) {
        q = r->next;
        free(r->msg);
        free(r);
      }
    }
    if (p->away) {
      free(p->away);
    }
    free(p);
  }
}

/* Remove the color control codes that mIRC,pIRCh etc use to make
 * their client seem so fecking cool! (Sorry, Khaled, you are a nice
 * guy, but when you added this feature you forced people to either
 * use your *SHAREWARE* client or face screenfulls of crap!)
 */
static void
strip_mirc_codes(int flags, char *text)
{
  char *dd = text;

  while (*text) {
    switch (*text) {
      case 2:                  /* Bold text */
        if (flags & STRIP_BOLD) {
          text++;
          continue;
        }
        break;
      case 3:                  /* mIRC colors? */
        if (flags & STRIP_COLOR) {
          if (egg_isdigit(text[1])) {   /* Is the first char a number? */
            text += 2;          /* Skip over the ^C and the first digit */
            if (egg_isdigit(*text))
              text++;           /* Is this a double digit number? */
            if (*text == ',') { /* Do we have a background color next? */
              if (egg_isdigit(text[1]))
                text += 2;      /* Skip over the first background digit */
              if (egg_isdigit(*text))
                text++;         /* Is it a double digit? */
            }
          } else
            text++;
          continue;
        }
        break;
      case 7:
        if (flags & STRIP_BELLS) {
          text++;
          continue;
        }
        break;
      case 0x16:               /* Reverse video */
        if (flags & STRIP_REV) {
          text++;
          continue;
        }
        break;
      case 0x1f:               /* Underlined text */
        if (flags & STRIP_UNDER) {
          text++;
          continue;
        }
        break;
      case 033:
        if (flags & STRIP_ANSI) {
          text++;
          if (*text == '[') {
            text++;
            while ((*text == ';') || egg_isdigit(*text))
              text++;
            if (*text)
              text++;           /* also kill the following char */
          }
          continue;
        }
        break;
    }
    *dd++ = *text++;            /* Move on to the next char */
  }
  *dd = 0;
}

static void
append_line(int idx, char *line)
{
  struct msgq *p = NULL, *q = NULL;
  struct chat_info *c = dcc[idx].u.chat;

  if (c->current_lines > 1000) {
    /* They're probably trying to fill up the bot nuke the sods :) */
    for (p = c->buffer; p; p = q) {
      q = p->next;
      free(p->msg);
      free(p);
    }
    c->buffer = 0;
    dcc[idx].status &= ~STAT_PAGE;
    do_boot(idx, conf.bot->nick, "too many pages - senq full");
    return;
  }

  size_t l = strlen(line);

  if ((c->line_count < c->max_line) && (c->buffer == NULL)) {
    c->line_count++;
    tputs(dcc[idx].sock, line, l);
  } else {
    c->current_lines++;
    if (c->buffer == NULL)
      q = NULL;
    else
      for (q = c->buffer; q->next; q = q->next) ;

    p = (struct msgq *) my_calloc(1, sizeof(struct msgq));

    p->len = l;
    p->msg = (char *) my_calloc(1, l + 1);
    p->next = NULL;
    strcpy(p->msg, line);
    if (q == NULL)
      c->buffer = p;
    else
      q->next = p;
  }
}

static void
out_dcc_general(int idx, char *buf, void *x)
{
  char *y = buf;

  if (dcc[idx].type == &DCC_CHAT) {
    register struct chat_info *p = (struct chat_info *) x;

    strip_mirc_codes(p->strip_flags, buf);
  }
  if (dcc[idx].status & STAT_TELNET)
    y = add_cr(buf);
  if (!dcc[idx].bot && dcc[idx].status & STAT_PAGE)
    append_line(idx, y);
  else
    tputs(dcc[idx].sock, y, strlen(y));
}

static struct dcc_table DCC_CHAT_SECPASS = {
  "CHAT_SECPASS",
  0,
  eof_dcc_general,
  dcc_chat_secpass,
  &auth_timeout,
  tout_dcc_chat_secpass,
  display_dcc_chat_secpass,
  kill_dcc_general,
  out_dcc_general,
  NULL
};

//su drops us here
static void
dcc_chat_pass(int idx, char *buf, int atr)
{
  if (!atr)
    return;

  char *pass = NULL;

  strip_telnet(dcc[idx].sock, buf, &atr);
  atr = dcc[idx].user ? dcc[idx].user->flags : 0;

  pass = newsplit(&buf);

  if (dcc[idx].user->bot) {
    if (!egg_strcasecmp(pass, "neg!")) {		/* we're the hub */
      link_parse(idx, buf);
    } else if (!egg_strcasecmp(pass, "neg.")) {		/* we're done, link up! */
      dcc[idx].type = &DCC_BOT_NEW;
      dcc[idx].u.bot = (struct bot_info *) my_calloc(1, sizeof(struct bot_info));
      dcc[idx].status = STAT_CALLED;
      dprintf(idx, "goodbye!\n");
      greet_new_bot(idx);
      if (conf.bot->hub)
        send_timesync(idx);
    } else if (!egg_strcasecmp(pass, "neg")) {
      int snum = findanysnum(dcc[idx].sock);

      if (snum >= 0) {
        char *hash = newsplit(&buf);

        if (strcmp(dcc[idx].shahash, hash)) {
          putlog(LOG_WARN, "*", "%s attempted to negotiate an encryption with an invalid hash.", dcc[idx].nick);
          killsock(dcc[idx].sock);
          lostdcc(idx);
          return;
        }
        int type = atoi(newsplit(&buf)), i = -1;

        /* verify we have that type and then initiate it */
        if ((i = link_find_by_type(type)) == -1) {
          putlog(LOG_WARN, "*", "%s attempted to link with an invalid encryption. (%d)", dcc[idx].nick, type);
          killsock(dcc[idx].sock);
          lostdcc(idx);
          return;
        }
        socklist[snum].enclink = i;

        link_link(idx, -1, i, FROM);
      }
    } else {
      /* Invalid password/digest on hub */
      putlog(LOG_WARN, "*", "%s failed encrypted link handshake.", dcc[idx].nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }

    return;
  }
  /* else !bot */
  int passok = u_pass_match(dcc[idx].user, pass);
  bool do_obscure = (!passok && auth_obscure) ? 1 : 0;

  if (passok || do_obscure) {
    if (do_obscure)
      dcc[idx].wrong_pass = 1;

    if (dccauth || do_obscure) { 
      char randstr[51] = "";

      make_rand_str(randstr, 50);
      makehash(dcc[idx].user, randstr, dcc[idx].hash, MD5_HASH_LENGTH + 1);

      dcc[idx].type = &DCC_CHAT_SECPASS;
      dcc[idx].timeval = now;
      dprintf(-dcc[idx].sock, "-Auth %s %s\n", randstr, conf.bot->nick);
    } else {
      dcc_chat_secpass(idx, pass, atr);
    }
  } else {
    dprintf(idx, "%s\n", response(RES_BADUSERPASS));
    putlog(LOG_MISC, "*", DCC_BADLOGIN, dcc[idx].nick, dcc[idx].host, dcc[idx].port);
    if (dcc[idx].u.chat->away) {        /* su from a dumb user */
      /* Turn echo back on for telnet sessions (send IAC WON'T ECHO). */
      if (dcc[idx].status & STAT_TELNET)
        dprintf(idx, TLN_IAC_C TLN_WONT_C TLN_ECHO_C "\n");
      dcc[idx].user = get_user_by_handle(userlist, dcc[idx].u.chat->away);
      strcpy(dcc[idx].nick, dcc[idx].u.chat->away);
      free(dcc[idx].u.chat->away);
      free(dcc[idx].u.chat->su_nick);
      dcc[idx].u.chat->away = NULL;
      dcc[idx].u.chat->su_nick = NULL;
      dcc[idx].type = &DCC_CHAT;

      dcc[idx].u.chat->channel = dcc[idx].u.chat->su_channel;
      if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
        botnet_send_join_idx(idx);
      chanout_but(-1, dcc[idx].u.chat->channel, DCC_JOIN, dcc[idx].nick);
    } else {
      killsock(dcc[idx].sock);
      lostdcc(idx);
    }
  }
}

struct dcc_table DCC_CHAT_PASS = {
  "CHAT_PASS",
  0,
  eof_dcc_general,
  dcc_chat_pass,
  &password_timeout,
  tout_dcc_chat_pass,
  display_dcc_chat_pass,
  kill_dcc_general,
  out_dcc_general,
  NULL
};


/* Make sure ansi code is just for color-changing
 */
static int
check_ansi(char *v)
{
  int count = 2;

  if (*v++ != '\033')
    return 1;
  if (*v++ != '[')
    return 1;
  while (*v) {
    if (*v == 'm')
      return 0;
    if ((*v != ';') && ((*v < '0') || (*v > '9')))
      return count;
    v++;
    count++;
  }
  return count;
}

int ansi_len(char *s)
{
  char *c = s;
  int count = 0;

  while (*c) {
    if (*c == 27) {
      c++;
      count++;

      if (*c == '[') {
        c++;
        count++;
        while ((*c != 'm')) {
          c++;
          count++;
        }
        c++;
        count++;
      }
    } else
      c++;
  }

  return count;
}

static void
eof_dcc_chat(int idx)
{
  putlog(LOG_MISC, "*", DCC_LOSTDCC, dcc[idx].nick, dcc[idx].host, dcc[idx].port);
  if (dcc[idx].u.chat->channel >= 0) {
    chanout_but(idx, dcc[idx].u.chat->channel, "*** %s lost dcc link.\n", dcc[idx].nick);
    if (dcc[idx].u.chat->channel < GLOBAL_CHANS)
      botnet_send_part_idx(idx, "lost dcc link");
  }
  check_chof(dcc[idx].nick, idx);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
dcc_chat(int idx, char *buf, int len)
{
  int i = 0;

  strip_telnet(dcc[idx].sock, buf, &len);
  if (buf[0] && (buf[0] != settings.dcc_prefix[0]) && !(dcc[idx].user && (dcc[idx].user->flags & USER_NOFLOOD)) &&
      detect_dcc_flood(&dcc[idx].timeval, dcc[idx].u.chat, idx))
    return;

  dcc[idx].timeval = now;
  if (buf[0]) {
    int nathan = 0, doron = 0, fixed = 0;
    char *v = buf, *d = buf;

    /* Check for beeps and cancel annoying ones */
    while (*v)
      switch (*v) {
        case 1:			/* CTCP ?! */
          v++;
          break;
        case 7:                /* Beep - no more than 3 */
          nathan++;
          if (nathan > 3)
            v++;
          else
            *d++ = *v++;
          break;
        case 8:                /* Backspace - for lame telnet's :) */
          if (d > buf) {
            d--;
          }
          v++;
          break;
        case 27:               /* ESC - ansi code? */
          doron = check_ansi(v);
          /* If it's valid, append a return-to-normal code at the end */
          if (!doron) {
            *d++ = *v++;
            fixed = 1;
          } else
            v += doron;
          break;
        case '\r':             /* Weird pseudo-linefeed */
          v++;
          break;
        default:
          *d++ = *v++;
      }
    if (fixed)
      strcpy(d, "\033[0m");
    else
      *d = 0;

    if (u_pass_match(dcc[idx].user, buf)) {     /* user said their password :) */
      dprintf(idx, "Sure you want that going to the partyline? ;) (msg to partyline halted.)\n");
    } else if (!strncmp(buf, "+Auth ", 6)) {    /* ignore extra +Auth lines */
    } else if ((!strncmp(buf, settings.dcc_prefix, strlen(settings.dcc_prefix))) || (dcc[idx].u.chat->channel < 0)) {
      if (!strncmp(buf, settings.dcc_prefix, strlen(settings.dcc_prefix)) && (dcc[idx].u.chat->channel >= 0))        /* strip '.' out */
        buf++;
      v = newsplit(&buf);
      rmspace(buf);
      check_bind_dcc(v, idx, buf);
    } else if (buf[0] == ',') {
      int me = 0;

      if ((buf[1] == 'm') && (buf[2] == 'e') && buf[3] == ' ')
        me = 1;
      for (i = 0; i < dcc_total; i++) {
       if (dcc[i].type) {
        int ok = 0;

        if (dcc[i].type->flags & DCT_MASTER) {
          if ((dcc[i].type != &DCC_CHAT) || (dcc[i].u.chat->channel >= 0))
            if ((i != idx) || (dcc[idx].status & STAT_ECHO))
              ok = 1;
        }
        if (ok) {
          struct userrec *u = get_user_by_handle(userlist, dcc[i].nick);

          if (u && (u->flags & USER_MASTER)) {
            if (me)
              dprintf(i, "-> %s%s\n", dcc[idx].nick, buf + 3);
            else
              dprintf(i, "-%s-> %s\n", dcc[idx].nick, buf + 1);
          }
        }
       }
      }
    } else if (buf[0] == '\'') {
      int me = 0;

      if ((buf[1] == 'm') && (buf[2] == 'e') && ((buf[3] == ' ') || (buf[3] == '\'') || (buf[3] == ',')))
        me = 1;
      for (i = 0; i < dcc_total; i++) {
        if (dcc[i].type && dcc[i].type->flags & DCT_CHAT) {
          if (me)
            dprintf(i, "=> %s%s\n", dcc[idx].nick, buf + 3);
          else
            dprintf(i, "=%s=> %s\n", dcc[idx].nick, buf + 1);
        }
      }
    } else { /* partyline chat */
      if (dcc[idx].u.chat->away != NULL)
        not_away(idx);
      /* Check for CTCP (/me) */
      if (!strncmp(buf, "CTCP_MESSAGE ", 13))		/* irssi */
        buf += 13;
      if (!strncmp(buf, "ACTION ", 7)) {
        buf += 7;
        check_bind_dcc("me", idx, buf);
      } else {		/* regular text */
        if (dcc[idx].status & STAT_ECHO)
          chanout_but(-1, dcc[idx].u.chat->channel, "<%s> %s\n", dcc[idx].nick, buf);
        else
          chanout_but(idx, dcc[idx].u.chat->channel, "<%s> %s\n", dcc[idx].nick, buf);
        botnet_send_chan(-1, conf.bot->nick, dcc[idx].nick, dcc[idx].u.chat->channel, buf);
      }
    }
  }
  if (dcc[idx].type == &DCC_CHAT)       /* Could have change to files */
    if (dcc[idx].status & STAT_PAGE)
      flush_lines(idx, dcc[idx].u.chat);
}

static void
display_dcc_chat(int idx, char *buf)
{
  size_t i = simple_sprintf(buf, "chat  flags: ");
  int colori = 0;

  buf[i++] = dcc[idx].status & STAT_CHAT ? 'C' : 'c';
  buf[i++] = dcc[idx].status & STAT_PARTY ? 'P' : 'p';
  buf[i++] = dcc[idx].status & STAT_TELNET ? 'T' : 't';
  buf[i++] = dcc[idx].status & STAT_ECHO ? 'E' : 'e';
  buf[i++] = dcc[idx].status & STAT_PAGE ? 'P' : 'p';
  if ((colori = coloridx(idx)))
    buf[i++] = colori == 1 ? 'A' : 'M';
#ifdef USE_IPV6
  if (sockprotocol(dcc[idx].sock) == AF_INET6 && dcc[idx].host6[0])
    buf[i++] = '6';
#endif /* USE_IPV6 */
  simple_sprintf(buf + i, "/%d", dcc[idx].u.chat->channel);
}

struct dcc_table DCC_CHAT = {
  "CHAT",
  DCT_CHAT | DCT_MASTER | DCT_SHOWWHO | DCT_VALIDIDX | DCT_SIMUL | DCT_CANBOOT | DCT_REMOTEWHO,
  eof_dcc_chat,
  dcc_chat,
  NULL,
  NULL,
  display_dcc_chat,
  kill_dcc_general,
  out_dcc_general,
  NULL
};

static int lasttelnets;
static char lasttelnethost[81];
static time_t lasttelnettime;

/* A modified detect_flood for incoming telnet flood protection.
 */
static bool
detect_telnet_flood(char *floodhost)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(get_user_by_host(floodhost), &fr, NULL);
  if (!flood_telnet_thr || glob_noflood(fr))
    return 0;                   /* No flood protection */
  if (egg_strcasecmp(lasttelnethost, floodhost)) {      /* New... */
    strcpy(lasttelnethost, floodhost);
    lasttelnettime = now;
    lasttelnets = 0;
    return 0;
  }
  if (lasttelnettime < now - flood_telnet_time) {
    /* Flood timer expired, reset it */
    lasttelnettime = now;
    lasttelnets = 0;
    return 0;
  }
  lasttelnets++;
  if (lasttelnets >= flood_telnet_thr) {        /* FLOOD! */
    /* Reset counters */
    lasttelnets = 0;
    lasttelnettime = 0;
    lasttelnethost[0] = 0;
    putlog(LOG_MISC, "*", "Telnet connection flood from %s!  Placing on ignore!", floodhost);
    addignore(floodhost, conf.bot->nick, "Telnet connection flood", now + (60 * ignore_time));
    return 1;
  }
  return 0;
}

static void dcc_telnet_dns_callback(int, void *, const char *, char **);

static void
dcc_telnet(int idx, char *buf, int ii)
{
  in_addr_t ip;
  port_t port;
  char s[UHOSTLEN + 1] = "";

  if (dcc_total + 1 > max_dcc) {
    int j;

    j = answer(dcc[idx].sock, s, &ip, &port, 0);
    if (j != -1) {
      dprintf(-j, "Sorry, too many connections already.\r\n");
      killsock(j);
    }
    return;
  }

  int sock = answer(dcc[idx].sock, s, &ip, &port, 0);

  while ((sock == -1) && (errno == EAGAIN))
    sock = answer(dcc[idx].sock, s, &ip, &port, 0);
/*. ssl_link ACCEPT_SSL should go here!!!! */
  if (sock < 0) {
    putlog(LOG_MISC, "*", DCC_FAILED, strerror(errno));
//    killsock(dcc[idx].sock);
    return;
  }
  /* Buffer data received on this socket.  */
  sockoptions(sock, EGG_OPTION_SET, SOCK_BUFFER);

#if SIZEOF_SHORT == 2
  if (port < 1024) {
#else
  if (port < 1024 || port > 65535) {
#endif
    putlog(LOG_BOTS, "*", DCC_BADSRC, s, port);
    killsock(sock);
    return;
  }

  char x[1024] = "";
  int i;

  putlog(LOG_DEBUG, "*", "Telnet connection: %s/%d", s, port);

  simple_snprintf(x, sizeof(x), "-telnet!telnet@%s", iptostr(htonl(ip)));

  if (match_ignore(x) || detect_telnet_flood(x)) {
    putlog(LOG_DEBUG, "*", "Ignored telnet connection from: %s", x);
    killsock(sock);
    return;
  }
  
  /* If a matching bot ip is found, it might still be on the ignore list as a host,
   * so we'll just reverse the ip anyway and check the ignores before
   * proceeding with user matching 
   */

  i = new_dcc(&DCC_DNSWAIT, sizeof(struct dns_info));

  dcc[i].addr = ip;
  dcc[i].sock = sock;
  dcc[i].user = get_user_by_host(x);		/* check for matching -telnet!telnet@ip */
  strcpy(dcc[i].host, s);
#ifdef USE_IPV6
  if (sockprotocol(sock) == AF_INET6)
    strcpy(dcc[i].host6, s);
#endif /* USE_IPV6 */
  dcc[i].port = port;
  dcc[i].timeval = now;
  strcpy(dcc[i].nick, "*");

  dcc[i].u.dns->ibuf = idx;

  int dns_id = egg_dns_reverse(s, 20, dcc_telnet_dns_callback, (void *) i);
  if (dns_id >= 0)
    dcc[i].dns_id = dns_id;
}

static void dcc_telnet_dns_callback(int id, void *client_data, const char *ip, char **hosts)
{
  int i = (int) client_data;

  if (!valid_dns_id(i, id))
    return;

  int j = -1, sock, idx = -1;
  char s2[UHOSTLEN + 20] = "";

  if (valid_idx(i))
    idx = dcc[i].u.dns->ibuf;

  strlcpy(dcc[i].host, hosts ? hosts[0] : ip, UHOSTLEN);

  simple_snprintf(s2, sizeof(s2), "-telnet!telnet@%s", dcc[i].host);

  if (match_ignore(s2)) {
    putlog(LOG_DEBUG, "*", "Ignored telnet connection from: %s", s2);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }

  if (!valid_idx(idx)) {
    putlog(LOG_BOTS, "*", "Lost listening socket while resolving %s", dcc[i].host);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }

  if (dcc[idx].host[0] == '@') {
    /* Restrict by hostname */
    if (!wild_match(dcc[idx].host + 1, dcc[i].host)) {
      putlog(LOG_BOTS, "*", DCC_BADHOST, dcc[i].host);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
    }
  }

/* .  ssl_link(dcc[i].sock, ACCEPT_SSL); */

  changeover_dcc(i, &DCC_IDENTWAIT, 0);
  dcc[i].timeval = now;
  dcc[i].u.ident_sock = dcc[idx].sock;

  if (!dcc[i].user)
    dcc[i].user = get_user_by_host(s2);		/* check for matching -telnet!telnet@host */
  
  if (hosts)
    putlog(LOG_MISC, "*", "Telnet connection: %s[%s]/%d", dcc[i].host, ip, dcc[i].port);
  else
    putlog(LOG_MISC, "*", "Telnet connection: %s/%d", dcc[i].host, dcc[i].port);

  sock = open_telnet((char *) ip, 113, 0);

  char s[UHOSTLEN] = "";

  if (sock < 0) {
    if (sock == -2)
      strcpy(s, "DNS lookup failed for ident");
    else
      strcpy(s, strerror(errno));
  } else {
    j = new_dcc(&DCC_IDENT, 0);
    if (j < 0) {
      killsock(sock);
      strcpy(s, "No Free DCC's");
    }
  }
  if (s[0]) {
    putlog(LOG_MISC, "*", DCC_IDENTFAIL, dcc[i].host, s);
    simple_snprintf(s, sizeof(s), "telnet@%s", dcc[i].host);
    dcc_telnet_got_ident(i, s);
    return;
  }
  dcc[j].sock = sock;
  dcc[j].port = 113;
  dcc[j].addr = dcc[i].addr;
  strcpy(dcc[j].host, dcc[i].host);
  strcpy(dcc[j].nick, "*");
  dcc[j].u.ident_sock = dcc[i].sock;
  dcc[j].user = dcc[i].user;
  dcc[j].timeval = now;
  dprintf(j, "%d, %d\n", dcc[i].port, dcc[idx].port);
}

static void
eof_dcc_telnet(int idx)
{
  putlog(LOG_MISC, "*", DCC_PORTDIE, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_telnet(int idx, char *buf)
{
  simple_sprintf(buf, "lstn  %d%s", dcc[idx].port, (dcc[idx].status & LSTN_PUBLIC) ? " pub" : "");
}

struct dcc_table DCC_TELNET = {
  "TELNET",
  DCT_LISTEN,
  eof_dcc_telnet,
  dcc_telnet,
  NULL,
  NULL,
  display_telnet,
  NULL,
  NULL,
  NULL
};

static void
eof_dcc_dupwait(int idx)
{
  putlog(LOG_BOTS, "*", DCC_LOSTDUP, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
dcc_dupwait(int idx, char *buf, int i)
{
  /* We just ignore any data at this point. */
  return;
}

/* We now check again. If the bot is still marked as duplicate, there is no
 * botnet lag we could push it on, so we just drop the connection.
 */
static void
timeout_dupwait(int idx)
{
  /* Still duplicate? */
  if (in_chain(dcc[idx].nick)) {
    char x[UHOSTLEN] = "";

    simple_snprintf(x, sizeof x, "%s!%s", dcc[idx].nick, dcc[idx].host);
    putlog(LOG_BOTS, "*", DCC_DUPLICATE, x);
    killsock(dcc[idx].sock);
    lostdcc(idx);
  } else {
    /* Ha! Now it's gone and we can grant this bot access. */
    dcc_telnet_pass(idx, dcc[idx].u.dupwait->atr);
  }
}

static void
display_dupwait(int idx, char *buf)
{
  simple_sprintf(buf, "wait  duplicate?");
}

static void
kill_dupwait(int idx, void *x)
{
  register struct dupwait_info *p = (struct dupwait_info *) x;

  if (p)
    free(p);
}

struct dcc_table DCC_DUPWAIT = {
  "DUPWAIT",
  DCT_VALIDIDX,
  eof_dcc_dupwait,
  dcc_dupwait,
  &dupwait_timeout,
  timeout_dupwait,
  display_dupwait,
  kill_dupwait,
  NULL,
  NULL
};

/* This function is called if a bot gets removed from the list. It checks
 * wether we have a pending duplicate connection for that bot and continues
 * with the login in that case.
 */
void
dupwait_notify(const char *who)
{
  for (register int idx = 0; idx < dcc_total; idx++)
    if (dcc[idx].type && (dcc[idx].type == &DCC_DUPWAIT) && !egg_strcasecmp(dcc[idx].nick, who)) {
      dcc_telnet_pass(idx, dcc[idx].u.dupwait->atr);
      break;
    }
}

static void
dcc_telnet_id(int idx, char *buf, int atr)
{
  char *nick = buf;

  strip_telnet(dcc[idx].sock, nick, &atr);

  if (nick[0] == '-') {
    nick++;
    dcc[idx].bot = 1;
  }

  nick[HANDLEN] = 0;

  dcc[idx].user = get_user_by_handle(userlist, nick);

  bool ok = 0;

  if (dcc[idx].user) {
    if (!dcc[idx].bot && dcc[idx].user->bot) {
      putlog(LOG_WARN, "*", "Refused %s (fake bot login for '%s')", dcc[idx].host, nick);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    }

    struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

    get_user_flagrec(dcc[idx].user, &fr, NULL);

    ok = 1;
    if (conf.bot->hub && !glob_huba(fr))
      ok = 0;
    
    if (!conf.bot->hub) {
      /* if I am a chanhub and they dont have +c then drop */
      if (ischanhub() && !glob_chuba(fr))
        ok = 0;
      if (!ischanhub())
        ok = 0;
    }
    if (!ok && glob_bot(fr))
      ok = 1;
  }

  if (!ok) {
    if (dcc[idx].user)
      putlog(LOG_BOTS, "*", "%s: %s!%s", ischanhub() ? DCC_REFUSED : DCC_REFUSEDNC, nick, dcc[idx].host);
    else
      putlog(LOG_BOTS, "*", DCC_INVHANDLE, dcc[idx].host, nick);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }
  correct_handle(nick);
  strcpy(dcc[idx].nick, nick);
  if (dcc[idx].user->bot) {
    if (!egg_strcasecmp(conf.bot->nick, dcc[idx].nick)) {
      putlog(LOG_BOTS, "*", DCC_MYBOTNETNICK, dcc[idx].host);
      killsock(dcc[idx].sock);
      lostdcc(idx);
      return;
    } else if (in_chain(dcc[idx].nick)) {

      dcc[idx].type = &DCC_DUPWAIT;
      dcc[idx].u.dupwait = (struct dupwait_info *) my_calloc(1, sizeof(struct dupwait_info));
      dcc[idx].u.dupwait->atr = atr;
      return;
    }
//    dcc[idx].u.enc = (struct enc_link_dcc *) my_calloc(1, sizeof(struct enc_link_dcc));
//    dcc[idx].u.enc->method_number = 0;
//    link_get_method(idx);
  } else {
  }

  dcc_telnet_pass(idx, atr);
}


static void
dcc_telnet_pass(int idx, int atr)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(dcc[idx].user, &fr, NULL);

  /* No password set? */
  if (!dcc[idx].user->bot && (u_pass_match(dcc[idx].user, "-"))) {
    dprintf(idx, "Can't telnet until you have a password set.\r\n");
    putlog(LOG_MISC, "*", DCC_NOPASS, dcc[idx].nick, dcc[idx].host);
    killsock(dcc[idx].sock);
    lostdcc(idx);
    return;
  }

  if (dcc[idx].type == &DCC_DUPWAIT) {
    free(dcc[idx].u.dupwait);
    dcc[idx].u.other = NULL;
  }

  dcc[idx].type = &DCC_CHAT_PASS;
  dcc[idx].timeval = now;

  if ((conf.bot->hub && !glob_huba(fr)) || (!conf.bot->hub && ischanhub() && !glob_chuba(fr)))
    dcc[idx].status |= STAT_PARTY;

  if (!dcc[idx].bot) {
    //bots dont need this
    dcc[idx].u.chat = (struct chat_info *) my_calloc(1, sizeof(struct chat_info));
    strcpy(dcc[idx].u.chat->con_chan, chanset ? chanset->dname : "*");
  }

  if (conf.bot->hub) {
    if (dcc[idx].bot) {
      /* negotiate a new linking scheme */
      int i = 0;
      char buf[1024] = "", rand[51] = "";
  
      make_rand_str(rand, 50);

      link_hash(idx, rand);

      for (i = 0; enclink[i].name; i++)
        simple_snprintf(buf, sizeof(buf), "%s%d ", buf[0] ? buf : "", enclink[i].type);

      dprintf(-dcc[idx].sock, "neg? %s %s\n", rand, buf);
    } else {
      /* Turn off remote telnet echo (send IAC WILL ECHO). */
      dprintf(idx, "\n%s" TLN_IAC_C TLN_WILL_C TLN_ECHO_C "\n", DCC_ENTERPASS);
    }
  } else
    dprintf(idx, "%s\n" TLN_IAC_C TLN_WILL_C TLN_ECHO_C, response(RES_PASSWORD));
}

static void
eof_dcc_telnet_id(int idx)
{
  putlog(LOG_MISC, "*", DCC_LOSTCON, dcc[idx].host, dcc[idx].port);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
timeout_dcc_telnet_id(int idx)
{
  putlog(LOG_MISC, "*", DCC_TTIMEOUT, dcc[idx].host);
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_telnet_id(int idx, char *buf)
{
  sprintf(buf, "t-in  waited %lis", now - dcc[idx].timeval);
}

struct dcc_table DCC_TELNET_ID = {
  "TELNET_ID",
  0,
  eof_dcc_telnet_id,
  dcc_telnet_id,
  &password_timeout,
  timeout_dcc_telnet_id,
  display_dcc_telnet_id,
  NULL,
  out_dcc_general,
  NULL
};

static void
dcc_socket(int idx, char *buf, int len)
{
}

static void
eof_dcc_socket(int idx)
{
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

static void
display_dcc_socket(int idx, char *buf)
{
  strcpy(buf, "sock  (stranded)");
}

struct dcc_table DCC_SOCKET = {
  "SOCKET",
  DCT_VALIDIDX,
  eof_dcc_socket,
  dcc_socket,
  NULL,
  NULL,
  display_dcc_socket,
  NULL,
  NULL,
  NULL
};

void
dcc_identwait(int idx, char *buf, int len)
{
  /* Ignore anything now */
}

void
eof_dcc_identwait(int idx)
{
  putlog(LOG_MISC, "*", DCC_LOSTCONN, dcc[idx].host, dcc[idx].port);
  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_IDENT) && (dcc[i].u.ident_sock == dcc[idx].sock)) {
      killsock(dcc[i].sock);    /* Cleanup ident socket */
      dcc[i].u.other = 0;
      lostdcc(i);
      break;
    }
  killsock(dcc[idx].sock);      /* Cleanup waiting socket */
  dcc[idx].u.other = 0;
  lostdcc(idx);
}

static void
display_dcc_identwait(int idx, char *buf)
{
  sprintf(buf, "idtw  waited %lis", now - dcc[idx].timeval);
}

struct dcc_table DCC_IDENTWAIT = {
  "IDENTWAIT",
  0,
  eof_dcc_identwait,
  dcc_identwait,
  NULL,
  NULL,
  display_dcc_identwait,
  NULL,
  NULL,
  NULL
};

void
dcc_ident(int idx, char *buf, int len)
{
  char ident_response[512] = "", uid[512] = "", buf1[UHOSTLEN] = "";

  sscanf(buf, "%*[^:]:%[^:]:%*[^:]:%[^\n]\n", ident_response, uid);
  rmspace(ident_response);
  if (ident_response[0] != 'U') {
    dcc[idx].timeval = now;
    return;
  }
  rmspace(uid);
  uid[20] = 0;                  /* 20 character ident max */
  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_IDENTWAIT) && (dcc[i].sock == dcc[idx].u.ident_sock)) {
      simple_snprintf(buf1, sizeof(buf1), "%s@%s", uid, dcc[idx].host);
      dcc_telnet_got_ident(i, buf1);
    }
  dcc[idx].u.other = 0;
  killsock(dcc[idx].sock);
  lostdcc(idx);
}

void
eof_dcc_ident(int idx)
{
  char buf[UHOSTLEN] = "";

  for (int i = 0; i < dcc_total; i++)
    if (dcc[i].type && (dcc[i].type == &DCC_IDENTWAIT) && (dcc[i].sock == dcc[idx].u.ident_sock)) {
      putlog(LOG_MISC, "*", DCC_EOFIDENT);
      simple_snprintf(buf, sizeof(buf), "telnet@%s", dcc[idx].host);
      dcc_telnet_got_ident(i, buf);
    }
  killsock(dcc[idx].sock);
  dcc[idx].u.other = 0;
  lostdcc(idx);
}

static void
display_dcc_ident(int idx, char *buf)
{
  simple_sprintf(buf, "idnt  (sock %d)", dcc[idx].u.ident_sock);
}

struct dcc_table DCC_IDENT = {
  "IDENT",
  0,
  eof_dcc_ident,
  dcc_ident,
  &identtimeout,
  eof_dcc_ident,
  display_dcc_ident,
  NULL,
  NULL,
  NULL
};

static void
dcc_telnet_got_ident(int i, char *host)
{
  int idx;

  for (idx = 0; idx < dcc_total; idx++)
    if (dcc[i].type && (dcc[idx].type == &DCC_TELNET) && (dcc[idx].sock == dcc[i].u.ident_sock))
      break;

  dcc[i].u.other = 0;

  if (idx == dcc_total || !dcc[idx].type) {
    putlog(LOG_MISC, "*", DCC_LOSTIDENT);
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }

  strlcpy(dcc[i].host, host, UHOSTLEN);

  char shost[UHOSTLEN + 20] = "", sip[UHOSTLEN + 20] = "";
  char *p = strchr(host, '@');
  *p = 0;

  simple_snprintf(shost, sizeof(shost), "-telnet!%s", dcc[i].host);
  simple_snprintf(sip, sizeof(sip), "-telnet!%s@%s", host, iptostr(htonl(dcc[i].addr)));

  if (match_ignore(shost) || match_ignore(sip)) {
    putlog(LOG_DEBUG, "*", "Ignored telnet connection from: %s[%s]",dcc[i].host, iptostr(htonl(dcc[i].addr)));
    killsock(dcc[i].sock);
    lostdcc(i);
    return;
  }
  
  if (protect_telnet) {
    struct userrec *u = NULL;
    bool ok = 1;

    u = dcc[i].user;
    if (!u)
      u = get_user_by_host(sip);			/* Check for -telnet!ident@ip */
    if (!u)
      u = get_user_by_host(shost);		/* Check for -telnet!ident@host */
    if (!u)
      ok = 0;

    if (ok && u && conf.bot->hub && !(u->flags & USER_HUBA))
      ok = 0;
    /* if I am a chanhub and they dont have +c then drop */
    if (ok && (!conf.bot->hub && ischanhub() && u && !(u->flags & USER_CHUBA)))
      ok = 0;
/*    else if (!(u->flags & USER_PARTY))
    ok = 0; */
    if (!ok && u && u->bot)
      ok = 1;
    if (!ok && (dcc[idx].status & LSTN_PUBLIC))
      ok = 1;
    if (!ok) {
      putlog(LOG_MISC, "*", DCC_NOACCESS, dcc[i].host);
      killsock(dcc[i].sock);
      lostdcc(i);
      return;
    }
  }

  /* Do not buffer data anymore. All received and stored data is passed
   * over to the dcc functions from now on.  */
  sockoptions(dcc[i].sock, EGG_OPTION_UNSET, SOCK_BUFFER);

  dcc[i].type = &DCC_TELNET_ID;

  /* Copy acceptable-nick/host mask */
  dcc[i].status = (STAT_TELNET | STAT_ECHO | STAT_COLOR | STAT_BANNER | STAT_CHANNELS | STAT_BOTS | STAT_WHOM);

  /* Copy acceptable-nick/host mask */
  strlcpy(dcc[i].nick, dcc[idx].host, HANDLEN);
  dcc[i].timeval = now;

  dcc[i].u.other = NULL;
  /* This is so we dont tell someone doing a portscan anything
   * about ourselves. <cybah>
   */
/* n  ssl_link(dcc[i].sock, ACCEPT_SSL); */
  if (conf.bot->hub)
    dprintf(i, " \n");			/* represents hub that support new linking scheme */
  else
    dprintf(i, "%s\n", response(RES_USERNAME));
}

void create_terminal_dcc() {
  int n = new_dcc(&DCC_CHAT, sizeof(struct chat_info));

  dcc[n].addr = iptolong(getmyip());
  dcc[n].sock = STDOUT;
  dcc[n].timeval = now;
  dcc[n].u.chat->con_flags = conmask | LOG_ALL;
  dcc[n].u.chat->strip_flags = STRIP_ALL;
  dcc[n].status = STAT_ECHO;
  strcpy(dcc[n].nick, "HQ");
  strcpy(dcc[n].host, "llama@console");
  dcc[n].user = get_user_by_handle(userlist, dcc[n].nick);
  /* Make sure there's an innocuous HQ user if needed */
  if (!dcc[n].user) {
    userlist = adduser(userlist, dcc[n].nick, "none", "-", USER_ADMIN | USER_OWNER | USER_MASTER | USER_VOICE | USER_OP | USER_PARTY | USER_CHUBA | USER_HUBA, 0);
    dcc[n].user = get_user_by_handle(userlist, dcc[n].nick);
  }
  setsock(STDOUT, 0);          /* Entry in net table */
  dprintf(n, "\n### ENTERING DCC CHAT SIMULATION ###\n\n");
  dcc_chatter(n);
}

void check_expired_dcc()
{
  for (int idx = 0; idx < dcc_total; ++idx) {
    if (dcc[idx].type && dcc[idx].type->timeout_val &&
        ((now - dcc[idx].timeval) > *(dcc[idx].type->timeout_val))) {
      if (dcc[idx].type->timeout)
        dcc[idx].type->timeout(idx);
      else if (dcc[idx].type->eof)
        dcc[idx].type->eof(idx);
      else
        continue;
      /* Only timeout 1 socket per cycle, too risky for more */
      return;
    }
  }
}

void check_expired_simuls() {
  for (int idx = 0; idx < dcc_total; idx++) {
    if (dcc[idx].type && dcc[idx].simul >= 0) {
      if ((now - dcc[idx].simultime) >= 100) { /* expire simuls after 100 seconds (re-uses idx, so it wont fill up) */
        dcc[idx].simul = -1;
        lostdcc(idx);
      }
    }
  }
}

void check_autoaway() {
  char autoaway[51] = "";

  simple_snprintf(autoaway, sizeof(autoaway), "Auto away after %d minutes.", dcc_autoaway / 60);

  for (int idx = 0; idx < dcc_total; ++idx)
    if (dcc[idx].type && dcc[idx].type == &DCC_CHAT &&
        !(dcc[idx].u.chat->away) && ((now - dcc[idx].timeval) >= dcc_autoaway))
      set_away(idx, autoaway);
}

