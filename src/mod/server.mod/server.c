/*
 * server.c -- part of server.mod
 *   basic irc server support
 *
 */


#include "src/common.h"
#include "src/set.h"
#include "src/botmsg.h"
#include "src/rfc1459.h"
#include "src/settings.h"
#include "src/match.h"
#include "src/tclhash.h"
#include "src/users.h"
#include "src/userrec.h"
#include "src/main.h"
#include "src/response.h"
#include "src/misc.h"
#include "src/chanprog.h"
#include "src/net.h"
#include "src/auth.h"
#include "src/adns.h"
#include "src/socket.h"
#include "src/egg_timer.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/ctcp.mod/ctcp.h"
#include "src/mod/irc.mod/irc.h"
#include "server.h"
#include <stdarg.h>

bool floodless = 0;		/* floodless iline? */
int ctcp_mode;
int serv = -1;		/* sock # of server currently */
int servidx = -1;		/* idx of server */
char newserver[121] = "";	/* new server? */
port_t newserverport = 0;		/* new server port? */
char newserverpass[121] = "";	/* new server password? */
static char serverpass[121] = "";
static time_t trying_server;	/* trying to connect to a server right now? */
int curserv = 999;		/* current position in server list: */
port_t curservport = 0;
rate_t flood_msg = { 5, 60 };
rate_t flood_ctcp = { 3, 60 };

char me[UHOSTLEN + NICKLEN] = "";
char meip[UHOSTLEN + NICKLEN] = "";

char botuserhost[UHOSTLEN] = "";	/* bot's user@host (refreshed whenever the bot joins a channel) */
					/* may not be correct user@host BUT it's how the server sees it */
char botuserip[UHOSTLEN] = "";		/* bot's user@host with the ip. */

static bool keepnick = 1;		/* keep trying to regain my intended
				   nickname? */
static bool nick_juped = 0;	/* True if origbotname is juped(RPL437) (dw) */
bool quiet_reject = 1;	/* Quietly reject dcc chat or sends from
				   users without access? */
static time_t waiting_for_awake;	/* set when i unidle myself, cleared when
				   i get the response */
time_t server_online;	/* server connection time */
char botrealname[121] = "A deranged product of evil coders.";	/* realname of bot */
static time_t server_timeout = 15;	/* server timeout for connecting */
struct server_list *serverlist = NULL;	/* old-style queue, still used by
					   server list */
time_t cycle_time;			/* cycle time till next server connect */
port_t default_port = 6667;		/* default IRC port */
bool trigger_on_ignore;	/* trigger bindings if user is ignored ? */
int answer_ctcp = 1;		/* answer how many stacked ctcp's ? */
static bool check_mode_r;	/* check for IRCNET +r modes */
static int net_type = NETT_EFNET;
static bool resolvserv;		/* in the process of resolving a server host */
static time_t lastpingtime;	/* IRCNet LAGmeter support -- drummer */
static char stackablecmds[511] = "";
static char stackable2cmds[511] = "";
static time_t last_time;
static bool use_penalties;
static int use_fastdeq;
size_t nick_len = 9;			/* Maximal nick length allowed on the network. */

static bool double_mode = 0;		/* allow a msgs to be twice in a queue? */
static bool double_server = 0;
static bool double_help = 0;
static bool double_warned = 0;

static void empty_msgq(void);
static void disconnect_server(int, int);
static int calc_penalty(char *);
static bool fast_deq(int);
static char *splitnicks(char **);
static void msgq_clear(struct msgq_head *qh);
static int stack_limit = 4;

/* New bind tables. */
static bind_table_t *BT_raw = NULL, *BT_msg = NULL;
bind_table_t *BT_ctcr = NULL, *BT_ctcp = NULL, *BT_msgc = NULL;

#include "servmsg.c"

#define MAXPENALTY 10

/* Number of seconds to wait between transmitting queued lines to the server
 * lower this value at your own risk.  ircd is known to start flood control
 * at 512 bytes/2 seconds.
 */
#define msgrate 2

/* Maximum messages to store in each queue. */
static int maxqmsg = 300;
static struct msgq_head mq, hq, modeq;
static int burst;

#include "cmdsserv.c"


/*
 *     Bot server queues
 */

/* Called periodically to shove out another queued item.
 *
 * 'mode' queue gets priority now.
 *
 * Most servers will allow 'busts' of upto 5 msgs, so let's put something
 * in to support flushing modeq a little faster if possible.
 * Will send upto 4 msgs from modeq, and then send 1 msg every time
 * it will *not* send anything from hq until the 'burst' value drops
 * down to 0 again (allowing a sudden mq flood to sneak through).
 */
static void deq_msg()
{
  bool ok = 0;

  /* now < last_time tested 'cause clock adjustments could mess it up */
  if ((now - last_time) >= msgrate || now < (last_time - 90)) {
    last_time = now;
    if (burst > 0)
      burst--;
    ok = 1;
  }
  if (serv < 0)
    return;

  struct msgq *q = NULL;

  /* Send upto 4 msgs to server if the *critical queue* has anything in it */
  if (modeq.head) {
    while (modeq.head && (burst < 4) && ((last_time - now) < MAXPENALTY)) {
      if (!modeq.head)
        break;
      if (fast_deq(DP_MODE)) {
        burst++;
        continue;
      }
      write_to_server(modeq.head->msg, modeq.head->len);
      if (debug_output)
        putlog(LOG_SRVOUT, "@", "[m->] %s", modeq.head->msg);
      modeq.tot--;
      last_time += calc_penalty(modeq.head->msg);
      q = modeq.head->next;
      free(modeq.head->msg);
      free(modeq.head);
      modeq.head = q;
      burst++;
    }
    if (!modeq.head)
      modeq.last = 0;
    return;
  }
  /* Send something from the normal msg q even if we're slightly bursting */
  if (burst > 1)
    return;
  if (mq.head) {
    burst++;
    if (fast_deq(DP_SERVER))
      return;
    write_to_server(mq.head->msg, mq.head->len);
    if (debug_output) {
      putlog(LOG_SRVOUT, "@", "[s->] %s", mq.head->msg);
    }
    mq.tot--;
    last_time += calc_penalty(mq.head->msg);
    q = mq.head->next;
    free(mq.head->msg);
    free(mq.head);
    mq.head = q;
    if (!mq.head)
      mq.last = NULL;
    return;
  }
  /* Never send anything from the help queue unless everything else is
   * finished.
   */
  if (!hq.head || burst || !ok)
    return;
  if (fast_deq(DP_HELP))
    return;
  write_to_server(hq.head->msg, hq.head->len);
  if (debug_output) {
    putlog(LOG_SRVOUT, "@", "[h->] %s", hq.head->msg);
  }
  hq.tot--;
  last_time += calc_penalty(hq.head->msg);
  q = hq.head->next;
  free(hq.head->msg);
  free(hq.head);
  hq.head = q;
  if (!hq.head)
    hq.last = NULL;
}

static int calc_penalty(char * msg)
{
  if (!use_penalties && net_type != NETT_UNDERNET && net_type != NETT_HYBRID_EFNET)
    return 0;

  char *cmd = NULL, *par1 = NULL, *par2 = NULL, *par3 = NULL;
  register int penalty, i, ii;

  cmd = newsplit(&msg);
  if (msg)
    i = strlen(msg);
  else
    i = strlen(cmd);
  last_time -= 2; /* undo eggdrop standard flood prot */
  if (net_type == NETT_UNDERNET || net_type == NETT_HYBRID_EFNET) {
    last_time += (2 + i / 120);
    return 0;
  }
  penalty = (1 + i / 100);
  if (!egg_strcasecmp(cmd, "KICK")) {
    par1 = newsplit(&msg); /* channel */
    par2 = newsplit(&msg); /* victim(s) */
    par3 = splitnicks(&par2);
    penalty++;
    while (strlen(par3) > 0) {
      par3 = splitnicks(&par2);
      penalty++;
    }
    ii = penalty;
    par3 = splitnicks(&par1);
    while (strlen(par1) > 0) {
      par3 = splitnicks(&par1);
      penalty += ii;
    }
  } else if (!egg_strcasecmp(cmd, "MODE")) {
    i = 0;
    par1 = newsplit(&msg); /* channel */
    par2 = newsplit(&msg); /* mode(s) */
    if (!strlen(par2))
      i++;
    while (strlen(par2) > 0) {
      if (strchr("ntimps", par2[0]))
        i += 3;
      else if (!strchr("+-", par2[0]))
        i += 1;
      par2++;
    }
    while (strlen(msg) > 0) {
      newsplit(&msg);
      i += 2;
    }
    ii = 0;
    while (strlen(par1) > 0) {
      splitnicks(&par1);
      ii++;
    }
    penalty += (ii * i);
  } else if (!egg_strcasecmp(cmd, "TOPIC")) {
    penalty++;
    par1 = newsplit(&msg); /* channel */
    par2 = newsplit(&msg); /* topic */
    if (strlen(par2) > 0) {  /* topic manipulation => 2 penalty points */
      penalty += 2;
      par3 = splitnicks(&par1);
      while (strlen(par1) > 0) {
        par3 = splitnicks(&par1);
        penalty += 2;
      }
    }
  } else if (!egg_strcasecmp(cmd, "PRIVMSG") ||
	     !egg_strcasecmp(cmd, "NOTICE")) {
    par1 = newsplit(&msg); /* channel(s)/nick(s) */
    /* Add one sec penalty for each recipient */
    while (strlen(par1) > 0) {
      splitnicks(&par1);
      penalty++;
    }
  } else if (!egg_strcasecmp(cmd, "WHO")) {
    par1 = newsplit(&msg); /* masks */
    par2 = par1;
    while (strlen(par1) > 0) {
      par2 = splitnicks(&par1);
      if (strlen(par2) > 4)   /* long WHO-masks receive less penalty */
        penalty += 3;
      else
        penalty += 5;
    }
  } else if (!egg_strcasecmp(cmd, "AWAY")) {
    if (strlen(msg) > 0)
      penalty += 2;
    else
      penalty += 1;
  } else if (!egg_strcasecmp(cmd, "INVITE")) {
    /* Successful invite receives 2 or 3 penalty points. Let's go
     * with the maximum.
     */
    penalty += 3;
  } else if (!egg_strcasecmp(cmd, "JOIN")) {
    penalty += 2;
  } else if (!egg_strcasecmp(cmd, "PART")) {
    penalty += 4;
  } else if (!egg_strcasecmp(cmd, "VERSION")) {
    penalty += 2;
  } else if (!egg_strcasecmp(cmd, "TIME")) {
    penalty += 2;
  } else if (!egg_strcasecmp(cmd, "TRACE")) {
    penalty += 2;
  } else if (!egg_strcasecmp(cmd, "NICK")) {
    penalty += 3;
  } else if (!egg_strcasecmp(cmd, "ISON")) {
    penalty += 1;
  } else if (!egg_strcasecmp(cmd, "WHOIS")) {
    penalty += 2;
  } else if (!egg_strcasecmp(cmd, "DNS")) {
    penalty += 2;
  } else
    penalty++; /* just add standard-penalty */
  /* Shouldn't happen, but you never know... */
  if (penalty > 99)
    penalty = 99;
  if (penalty < 2) {
    putlog(LOG_SRVOUT, "*", "Penalty < 2sec, that's impossible!");
    penalty = 2;
  }
  if (debug_output && penalty != 0)
    putlog(LOG_SRVOUT, "*", "Adding penalty: %i", penalty);
  return penalty;
}

char *splitnicks(char **rest)
{
  if (!rest)
    return *rest = "";

  register char *o = *rest, *r = NULL;

  while (*o == ' ')
    o++;
  r = o;
  while (*o && *o != ',')
    o++;
  if (*o)
    *o++ = 0;
  *rest = o;
  return r;
}

static bool fast_deq(int which)
{
  if (!use_fastdeq)
    return 0;

  struct msgq_head *h = NULL;
  struct msgq *m = NULL, *nm = NULL;
  char msgstr[511] = "", nextmsgstr[511] = "", tosend[511] = "", victims[511] = "", stackable[511] = "",
       *msg = NULL, *nextmsg = NULL, *cmd = NULL, *nextcmd = NULL, *to = NULL, *nextto = NULL, *stckbl = NULL;
  int cmd_count = 0, stack_method = 1;
  size_t len;
  bool found = 0, doit = 0;

  switch (which) {
    case DP_MODE:
      h = &modeq;
      break;
    case DP_SERVER:
      h = &mq;
      break;
    case DP_HELP:
      h = &hq;
      break;
    default:
      return 0;
  }
  m = h->head;
  strlcpy(msgstr, m->msg, sizeof msgstr);
  msg = msgstr;
  cmd = newsplit(&msg);
  if (use_fastdeq > 1) {
    strlcpy(stackable, stackablecmds, sizeof stackable);
    stckbl = stackable;
    while (strlen(stckbl) > 0)
      if (!egg_strcasecmp(newsplit(&stckbl), cmd)) {
        found = 1;
        break;
      }
    /* If use_fastdeq is 2, only commands in the list should be stacked. */
    if (use_fastdeq == 2 && !found)
      return 0;
    /* If use_fastdeq is 3, only commands that are _not_ in the list
     * should be stacked.
     */
    if (use_fastdeq == 3 && found)
      return 0;
    /* we check for the stacking method (default=1) */
    strlcpy(stackable, stackable2cmds, sizeof stackable);
    stckbl = stackable;
    while (strlen(stckbl) > 0)
      if (!egg_strcasecmp(newsplit(&stckbl), cmd)) {
        stack_method = 2;
        break;
      }    
  }
  to = newsplit(&msg);
  len = strlen(to);
  simple_snprintf(victims, sizeof(victims), "%s", to);
  while (m) {
    nm = m->next;
    if (!nm)
      break;
    strlcpy(nextmsgstr, nm->msg, sizeof nextmsgstr);
    nextmsg = nextmsgstr;
    nextcmd = newsplit(&nextmsg);
    nextto = newsplit(&nextmsg);
    len = strlen(nextto);
    if ( strcmp(to, nextto) /* we don't stack to the same recipients */
        && !strcmp(cmd, nextcmd) && !strcmp(msg, nextmsg)
        && ((strlen(cmd) + strlen(victims) + strlen(nextto)
	     + strlen(msg) + 2) < 510)
        && (!stack_limit || cmd_count < stack_limit - 1)) {
      cmd_count++;
      if (stack_method == 1)
      	simple_snprintf(victims, sizeof(victims), "%s,%s", victims, nextto);
      else
      	simple_snprintf(victims, sizeof(victims), "%s %s", victims, nextto);
      doit = 1;
      m->next = nm->next;
      if (!nm->next)
        h->last = m;
      free(nm->msg);
      free(nm);
      h->tot--;
    } else
      m = m->next;
  }
  if (doit) {
    simple_snprintf(tosend, sizeof(tosend), "%s %s %s", cmd, victims, msg);
    len = strlen(tosend);
    write_to_server(tosend, len);
    m = h->head->next;
    free(h->head->msg);
    free(h->head);
    h->head = m;
    if (!h->head)
      h->last = 0;
    h->tot--;
    if (debug_output) {
      switch (which) {
        case DP_MODE:
          putlog(LOG_SRVOUT, "*", "[m=>] %s", tosend);
          break;
        case DP_SERVER:
          putlog(LOG_SRVOUT, "*", "[s=>] %s", tosend);
          break;
        case DP_HELP:
          putlog(LOG_SRVOUT, "*", "[h=>] %s", tosend);
          break;
      }
    }
    last_time += calc_penalty(tosend);
    return 1;
  }
  return 0;
}

/* Clean out the msg queues (like when changing servers).
 */
static void empty_msgq()
{
  msgq_clear(&modeq);
  msgq_clear(&mq);
  msgq_clear(&hq);
  burst = 0;
}

/* Use when sending msgs... will spread them out so there's no flooding.
 */
void queue_server(int which, char *buf, int len)
{
  /* Don't even BOTHER if there's no server online. */
  if (serv < 0)
    return;

  struct msgq_head *h = NULL, tempq;
  struct msgq *q = NULL;
  int qnext = 0;
  bool doublemsg = 0;

  switch (which) {
  case DP_MODE_NEXT:
    qnext = 1;
    /* Fallthrough */
  case DP_MODE:
    h = &modeq;
    tempq = modeq;
    if (double_mode)
      doublemsg = 1;
    break;

  case DP_SERVER_NEXT:
    qnext = 1;
    /* Fallthrough */
  case DP_SERVER:
    h = &mq;
    tempq = mq;
    if (double_server)
      doublemsg = 1;
    break;

  case DP_HELP_NEXT:
    qnext = 1;
    /* Fallthrough */
  case DP_HELP:
    h = &hq;
    tempq = hq;
    if (double_help)
      doublemsg = 1;
    break;

  default:
    putlog(LOG_MISC, "*", "!!! queuing unknown type to server!!");
    return;
  }

  if (h->tot < maxqmsg) {
    /* Don't queue msg if it's already queued?  */
    if (!doublemsg) {
      struct msgq *tq = NULL, *tqq = NULL;

      for (tq = tempq.head; tq; tq = tqq) {
	tqq = tq->next;
	if (!egg_strcasecmp(tq->msg, buf)) {
	  if (!double_warned) {
	    if (buf[len - 1] == '\n')
	      buf[len - 1] = 0;
	    debug1("msg already queued. skipping: %s", buf);
	    double_warned = 1;
	  }
	  return;
	}
      }
    }

    q = (struct msgq *) my_calloc(1, sizeof(struct msgq));
    if (qnext)
      q->next = h->head;
    else
      q->next = NULL;
    if (h->head) {
      if (!qnext)
        h->last->next = q;
    } else
      h->head = q;
    if (qnext)
       h->head = q;
    h->last = q;
    q->len = len;
    q->msg = (char *) my_calloc(1, len + 1);
    strlcpy(q->msg, buf, len + 1);
    h->tot++;
    h->warned = 0;
    double_warned = 0;
  } else {
    if (!h->warned) {
      switch (which) {   
	case DP_MODE_NEXT:
 	/* Fallthrough */
	case DP_MODE:
      putlog(LOG_MISC, "*", "!!! OVER MAXIMUM MODE QUEUE");
 	break;
    
	case DP_SERVER_NEXT:
 	/* Fallthrough */
 	case DP_SERVER:
	putlog(LOG_MISC, "*", "!!! OVER MAXIMUM SERVER QUEUE");
	break;
            
	case DP_HELP_NEXT:
	/* Fallthrough */
	case DP_HELP:
	putlog(LOG_MISC, "*", "!!! OVER MAXIMUM HELP QUEUE");
	break;
      }
    }
    h->warned = 1;
  }

  if (debug_output && !h->warned) {
    switch (which) {
    case DP_MODE:
      putlog(LOG_SRVOUT, "@", "[!m] %s", buf);
      break;
    case DP_SERVER:
      putlog(LOG_SRVOUT, "@", "[!s] %s", buf);
      break;
    case DP_HELP:
      putlog(LOG_SRVOUT, "@", "[!h] %s", buf);
      break;
    case DP_MODE_NEXT:
      putlog(LOG_SRVOUT, "@", "[!!m] %s", buf);
      break;
    case DP_SERVER_NEXT:
      putlog(LOG_SRVOUT, "@", "[!!s] %s", buf);
      break;
    case DP_HELP_NEXT:
      putlog(LOG_SRVOUT, "@", "[!!h] %s", buf);
      break;
    }
  }

  if (which == DP_MODE || which == DP_MODE_NEXT)
    deq_msg();		/* DP_MODE needs to be sent ASAP, flush if
			   possible. */
}

/* Add a new server to the server_list.
 */
void add_server(char *ss)
{
  struct server_list *x = NULL, *z = NULL;
#ifdef USE_IPV6
  char *r = NULL;
#endif /* USE_IPV6 */
  char *p = NULL, *q = NULL;

  for (z = serverlist; z && z->next; z = z->next);
  while (ss) {
    p = strchr(ss, ',');
    if (p)
      *p++ = 0;
    x = (struct server_list *) my_calloc(1, sizeof(struct server_list));

    x->next = 0;
    x->port = 0;
    if (z)
      z->next = x;
    else
      serverlist = x;
    z = x;
    q = strchr(ss, ':');
    if (!q) {
      x->port = default_port;
      x->pass = 0;
      x->name = strdup(ss);
    } else {
#ifdef USE_IPV6
      if (ss[0] == '[') {
        *ss++;
        q = strchr(ss, ']');
        *q++ = 0; /* intentional */
        r = strchr(q, ':');
        if (!r)
          x->port = default_port;
      }
#endif /* USE_IPV6 */
      *q++ = 0;
      x->name = (char *) my_calloc(1, q - ss);
      strcpy(x->name, ss);
      ss = q;
      q = strchr(ss, ':');
      if (!q) {
	x->pass = 0;
      } else {
	*q++ = 0;
        x->pass = strdup(q);
      }
#ifdef USE_IPV6
      if (!x->port) {
        x->port = atoi(ss);
      }
#else
      x->port = atoi(ss);
#endif /* USE_IPV6 */
    }
    ss = p;
  }
}

/* Clear out the given server_list.
 */
void clearq(struct server_list *xx)
{
  struct server_list *x = NULL;

  while (xx) {
    x = xx->next;
    if (xx->name)
      free(xx->name);
    if (xx->pass)
      free(xx->pass);
    free(xx);
    xx = x;
  }
}

/* Set botserver to the next available server.
 *
 * -> if (*ptr == -1) then jump to that particular server
 */
void next_server(int *ptr, char *servname, port_t *port, char *pass)
{
  struct server_list *x = serverlist;

  if (x == NULL)
    return;

  int i = 0;

  /* -1  -->  Go to specified server */
  if (*ptr == (-1)) {
    for (; x; x = x->next) {
      if (x->port == *port) {
	if (!egg_strcasecmp(x->name, servname)) {
	  *ptr = i;
	  return;
	}
      }
      i++;
    }
    /* Gotta add it: */
    x = (struct server_list *) my_calloc(1, sizeof(struct server_list));

    x->next = 0;
    x->name = strdup(servname);
    x->port = *port ? *port : default_port;
    if (pass && pass[0]) {
      x->pass = strdup(pass);
    } else
      x->pass = NULL;
    list_append((struct list_type **) (&serverlist), (struct list_type *) x);
    *ptr = i;
    return;
  }
  /* Find where i am and boogie */
  i = (*ptr);
  while (i > 0 && x != NULL) {
    x = x->next;
    i--;
  }
  if (x != NULL) {
    x = x->next;
    (*ptr)++;
  }				/* Go to next server */
  if (x == NULL) {
    x = serverlist;
    *ptr = 0;
  }				/* Start over at the beginning */
  strcpy(servname, x->name);
  *port = x->port ? x->port : default_port;
  if (x->pass)
    strcpy(pass, x->pass);
  else
    pass[0] = 0;
}

static void do_nettype(void)
{
  switch (net_type) {
  case NETT_EFNET:
    check_mode_r = 0;
    break;
  case NETT_IRCNET:
    check_mode_r = 1;
    use_fastdeq = 3;
    simple_snprintf(stackablecmds, sizeof(stackablecmds), "INVITE AWAY VERSION NICK");
    break;
  case NETT_UNDERNET:
    check_mode_r = 0;
    use_fastdeq = 2;
    simple_snprintf(stackablecmds, sizeof(stackablecmds), "PRIVMSG NOTICE TOPIC PART WHOIS USERHOST USERIP ISON");
    simple_snprintf(stackable2cmds, sizeof(stackable2cmds), "USERHOST USERIP ISON");
    break;
  case NETT_DALNET:
    check_mode_r = 0;
    use_fastdeq = 2;
    simple_snprintf(stackablecmds, sizeof(stackablecmds), "PRIVMSG NOTICE PART WHOIS WHOWAS USERHOST ISON WATCH DCCALLOW");
    simple_snprintf(stackable2cmds, sizeof(stackable2cmds), "USERHOST ISON WATCH");
    break;
  case NETT_HYBRID_EFNET:
    check_mode_r = 0;
    break;
  }
}

/*
 *     CTCP DCC CHAT functions
 */


static int sanitycheck_dcc(char *nick, char *from, char *ipaddy, char *port)
{
  /* According to the latest RFC, the clients SHOULD be able to handle
   * DNS names that are up to 255 characters long.  This is not broken.
   */

  char badaddress[16];
  in_addr_t ip = my_atoul(ipaddy);
  int prt = atoi(port);

  if (prt < 1) {
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) specified an impossible port of %u!",
           nick, from, prt);
    return 0;
  }
  simple_snprintf(badaddress, sizeof(badaddress), "%u.%u.%u.%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff,
          (ip >> 8) & 0xff, ip & 0xff);
  if (ip < (1 << 24)) {
    putlog(LOG_MISC, "*", "ALERT: (%s!%s) specified an impossible IP of %s!",
           nick, from, badaddress);
    return 0;
  }
  return 1;
}


static void dcc_chat_hostresolved(int);

/* This only handles CHAT requests, otherwise it's handled in filesys.
 */
static int ctcp_DCC_CHAT(char *nick, char *from, struct userrec *u, char *object, char *keyword, char *text)
{
  if (!ischanhub())
    return BIND_RET_LOG;

  char *action = NULL, *param = NULL, *ip = NULL, *prt = NULL;

  action = newsplit(&text);
  param = newsplit(&text);
  ip = newsplit(&text);
  prt = newsplit(&text);

  if (egg_strcasecmp(action, "CHAT") || egg_strcasecmp(object, botname) || !u)
    return BIND_RET_LOG;

  int i;
  bool ok = 1;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  get_user_flagrec(u, &fr, 0);

  if (ischanhub() && !glob_chuba(fr))
   ok = 0;
  if (dcc_total == max_dcc) {
    if (!quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, DCC_TOOMANYDCCS1);
    putlog(LOG_MISC, "*", DCC_TOOMANYDCCS2, "CHAT", param, nick, from);
  } else if (!ok) {
    if (!quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, DCC_REFUSED2);
    putlog(LOG_MISC, "*", "%s: %s!%s", ischanhub() ? DCC_REFUSED : DCC_REFUSEDNC, nick, from);
  } else if (u_pass_match(u, "-")) {
    if (!quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, DCC_REFUSED3);
    putlog(LOG_MISC, "*", "%s: %s!%s", DCC_REFUSED4, nick, from);
  } else if (atoi(prt) < 1024 || atoi(prt) > 65535) {
    /* Invalid port */
    if (!quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :%s (invalid port)\n", nick, DCC_CONNECTFAILED1);
    putlog(LOG_MISC, "*", "%s: CHAT (%s!%s)", DCC_CONNECTFAILED3, nick, from);
  } else {
    if (!sanitycheck_dcc(nick, from, ip, prt))
      return 1;

    i = new_dcc(&DCC_CHAT_PASS, sizeof(struct chat_info));

    if (i < 0) {
      putlog(LOG_MISC, "*", "DCC connection: CHAT (%s!%s)", nick, ip);
      return BIND_RET_BREAK;
    }
    dcc[i].addr = my_atoul(ip);
    dcc[i].port = atoi(prt);
    dcc[i].sock = -1;
    strcpy(dcc[i].nick, u->handle);
    strcpy(dcc[i].host, from);
    dcc[i].timeval = now;
    dcc[i].user = u;

    dcc_chat_hostresolved(i);

//    egg_dns_reverse(dcc[i].addr, 20, dcc_chat_dns_callback, (void *) i);
  }
  return BIND_RET_BREAK;
}

//static void tandem_relay_dns_callback(void *client_data, const char *host, char **ips)

static void dcc_chat_hostresolved(int i)
{
  char buf[512] = "", ip[512] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN | FR_ANYWH, 0, 0, 0 };

  simple_snprintf(buf, sizeof buf, "%d", dcc[i].port);

  egg_snprintf(ip, sizeof ip, "%lu", iptolong(htonl(dcc[i].addr)));
#ifdef USE_IPV6
  dcc[i].sock = getsock(0, AF_INET);
#else
  dcc[i].sock = getsock(0);
#endif /* USE_IPV6 */
  if (dcc[i].sock < 0 || open_telnet_dcc(dcc[i].sock, ip, buf) < 0) {
    strcpy(buf, strerror(errno));
    if (!quiet_reject)
      dprintf(DP_HELP, "NOTICE %s :%s (%s)\n", dcc[i].nick, DCC_CONNECTFAILED1, buf);
    putlog(LOG_MISC, "*", "%s: CHAT (%s!%s)", DCC_CONNECTFAILED2, dcc[i].nick, dcc[i].host);
    putlog(LOG_MISC, "*", "    (%s)", buf);
    killsock(dcc[i].sock);
    lostdcc(i);
  } else {
    bool ok = 1;
#ifdef HAVE_SSL
    ssl_link(dcc[i].sock, CONNECT_SSL);
#endif /* HAVE_SSL */

    dcc[i].status = STAT_ECHO;
    get_user_flagrec(dcc[i].user, &fr, 0);
    if (ischanhub() && !glob_chuba(fr))
     ok = 0;
    if (ok)
      dcc[i].status |= STAT_PARTY;
    strcpy(dcc[i].u.chat->con_chan, (chanset) ? chanset->dname : "*");
    dcc[i].timeval = now;
    /* Ok, we're satisfied with them now: attempt the connect */
    putlog(LOG_MISC, "*", "DCC connection: CHAT (%s!%s)", dcc[i].nick, dcc[i].host);
    dprintf(i, "%s\n", response(RES_USERNAME));
  }
  return;
}

/*
 *     Server timer functions
 */

static void server_secondly()
{
  if (cycle_time)
    cycle_time--;
  deq_msg();
  if (!resolvserv && serv < 0 && !trying_server)
    connect_server();
}

static void server_check_lag()
{
  if (server_online && !waiting_for_awake && !trying_server) {
    dprintf(DP_DUMP, "PING :%li\n", now);
    lastpingtime = now;
    waiting_for_awake = 1;
  }
}

static void server_5minutely()
{
  if (server_online && waiting_for_awake && ((now - lastpingtime) >= 300)) {
      /* Uh oh!  Never got pong from last time, five minutes ago!
       * Server is probably stoned.
       */
      disconnect_server(servidx, DO_LOST);
      putlog(LOG_SERV, "*", "Server got stoned; jumping...");
  }
}

void server_die()
{
  cycle_time = 100;
  if (server_online) {
    dprintf(-serv, "QUIT :%s\n", quit_msg[0] ? quit_msg : "");
    sleep(2); /* Give the server time to understand */
  }
  nuke_server(NULL);
}

/* A report on the module status.
 */
void server_report(int idx, int details)
{
  char s1[64] = "", s[128] = "";

  if (server_online) {
    dprintf(idx, "    Online as: %s%s%s (%s)\n", botname,
	    botuserhost[0] ? "!" : "", botuserhost[0] ? botuserhost : "",
	    botrealname);
    dprintf(idx, "    My userip: %s!%s\n", botname, botuserip);
    if (nick_juped)
      dprintf(idx, "    NICK IS JUPED: %s %s\n", origbotname,
	      keepnick ? "(trying)" : "");
    nick_juped = 0; /* WHY?? -- drummer */
    daysdur(now, server_online, s1);
    simple_snprintf(s, sizeof s, "(connected %s)", s1);
    if (server_lag && !waiting_for_awake) {
      if (server_lag == (-1))
	simple_snprintf(s1, sizeof s1, " (bad pong replies)");
      else
	simple_snprintf(s1, sizeof s1, " (lag: %ds)", server_lag);
      strcat(s, s1);
    }
  }
  if ((trying_server || server_online) && (servidx != (-1))) {
    dprintf(idx, "    Server %s:%d %s\n", dcc[servidx].host, dcc[servidx].port,
	    trying_server ? "(trying)" : s);
  } else
    dprintf(idx, "    No server currently.\n");
  if (modeq.tot)
    dprintf(idx, "    Mode queue is at %d%%, %d msgs\n",
            (int) ((float) (modeq.tot * 100.0) / (float) maxqmsg),
	    (int) modeq.tot);
  if (mq.tot)
    dprintf(idx, "    Server queue is at %d%%, %d msgs\n",
           (int) ((float) (mq.tot * 100.0) / (float) maxqmsg), (int) mq.tot);
  if (hq.tot)
    dprintf(idx, "    Help queue is at %d%%, %d msgs\n",
           (int) ((float) (hq.tot * 100.0) / (float) maxqmsg), (int) hq.tot);
  if (details) {
    dprintf(idx, "    Flood is: %d msg/%lus, %d ctcp/%lus\n",
	    flood_msg.count, flood_msg.time, flood_ctcp.count, flood_ctcp.time);
  }
}

static void msgq_clear(struct msgq_head *qh)
{
  register struct msgq *qq = NULL;

  for (register struct msgq *q = qh->head; q; q = qq) {
    qq = q->next;
    free(q->msg);
    free(q);
  }
  qh->head = qh->last = NULL;
  qh->tot = qh->warned = 0;
}

static cmd_t my_ctcps[] =
{
  {"DCC",	"",	(Function) ctcp_DCC_CHAT,		"server:DCC", LEAF},
  {NULL,	NULL,	NULL,			NULL, 0}
};

void server_init()
{
  strcpy(botrealname, "A deranged product of evil coders");
  strcpy(stackable2cmds, "USERHOST ISON");

  mq.head = hq.head = modeq.head = NULL;
  mq.last = hq.last = modeq.last = NULL;
  mq.tot = hq.tot = modeq.tot = 0;
  mq.warned = hq.warned = modeq.warned = 0;

  /*
   * Init of all the variables *must* be done in _start rather than
   * globally.
   */

  BT_msgc = bind_table_add("msgc", 3, "Ass", MATCH_FLAGS, 0); // Auth, chname, par
  BT_msg = bind_table_add("msg", 4, "ssUs", MATCH_FLAGS, 0);
  BT_raw = bind_table_add("raw", 2, "ss", 0, BIND_STACKABLE);
  BT_ctcr = bind_table_add("ctcr", 6, "ssUsss", 0, BIND_STACKABLE);
  BT_ctcp = bind_table_add("ctcp", 6, "ssUsss", 0, BIND_STACKABLE);

  add_builtins("raw", my_raw_binds);
  add_builtins("dcc", C_dcc_serv);
  add_builtins("ctcp", my_ctcps);

  timer_create_secs(1, "server_secondly", (Function) server_secondly);
  timer_create_secs(10, "server_10secondly", (Function) server_10secondly);
  timer_create_secs(30, "server_check_lag", (Function) server_check_lag);
  timer_create_secs(300, "server_5minutely", (Function) server_5minutely);
  timer_create_secs(60, "minutely_checks", (Function) minutely_checks);

  do_nettype();
}
