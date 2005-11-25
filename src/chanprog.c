/*
 * chanprog.c -- handles:
 *   rmspace()
 *   maintaining the server list
 *   revenge punishment
 *   timers, utimers
 *   telling the current programmed settings
 *   initializing a lot of stuff and loading the tcl scripts
 *
 */


#include "common.h"
#include "chanprog.h"
#include "settings.h"
#include "src/mod/channels.mod/channels.h"
#include "src/mod/server.mod/server.h"
#include "src/mod/share.mod/share.h"
#include "rfc1459.h"
#include "net.h"
#include "misc.h"
#include "users.h"
#include "userrec.h"
#include "main.h"
#include "debug.h"
#include "dccutil.h"
#include "botmsg.h"
#if HAVE_GETRUSAGE
#include <sys/resource.h>
#if HAVE_SYS_RUSAGE_H
#include <sys/rusage.h>
#endif
#endif
#include <sys/utsname.h>

struct chanset_t 	*chanset = NULL;	/* Channel list			*/
char 			admin[121] = "";	/* Admin info			*/
char 			origbotname[NICKLEN + 1] = "";
char 			botname[NICKLEN + 1] = "";	/* Primary botname		*/
port_t     		my_port = 0;
bool			reset_chans = 0;

/* Remove leading and trailing whitespaces.
 */
void rmspace(char *s)
{
  if (!s || !*s)
    return;

  register char *p = NULL, *q = NULL;

  /* Remove trailing whitespaces. */
  for (q = s + strlen(s) - 1; q >= s && egg_isspace(*q); q--);
  *(q + 1) = 0;

  /* Remove leading whitespaces. */
  for (p = s; egg_isspace(*p); p++);

  if (p != s)
    memmove(s, p, q - p + 2);
}

/* Returns memberfields if the nick is in the member list.
 */
Member *ismember(struct chanset_t *chan, const char *nick)
{
  return Member::Find(chan, nick);
}

/* Find a chanset by channel name as the server knows it (ie !ABCDEchannel)
 */
struct chanset_t *findchan(const char *name)
{
  register struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    if (!rfc_casecmp(chan->name, name))
      return chan;
  return NULL;
}

/* Find a chanset by display name (ie !channel)
 */
struct chanset_t *findchan_by_dname(const char *name)
{
  register struct chanset_t	*chan = NULL;

  for (chan = chanset; chan; chan = chan->next)
    if (!rfc_casecmp(chan->dname, name))
      return chan;
  return NULL;
}

/*
 *    "caching" functions
 */

/* Shortcut for get_user_by_host -- might have user record in one
 * of the channel caches.
 */
struct userrec *check_chanlist(const char *host)
{
  char				*nick = NULL, *uhost = NULL, buf[UHOSTLEN] = "";
  register Member		*m = NULL;
  register struct chanset_t	*chan = NULL;
  ptrlist<Member>::iterator _p;

  strlcpy(buf, host, sizeof buf);
  uhost = buf;
  nick = splitnick(&uhost);
  for (chan = chanset; chan; chan = chan->next) {
    PFOR(chan->channel.hmember, Member, m) {
      if (!rfc_casecmp(nick, m->nick) && !egg_strcasecmp(uhost, m->client->GetUHost()))
	return m->user;
    }
  }
  return NULL;
}

/* Shortcut for get_user_by_handle -- might have user record in channels
 */
struct userrec *check_chanlist_hand(const char *hand)
{
  register struct chanset_t	*chan = NULL;
  register Member		*m = NULL;
  ptrlist<Member>::iterator _p;

  for (chan = chanset; chan; chan = chan->next) {
    PFOR(chan->channel.hmember, Member, m) {
      if (m->user && !egg_strcasecmp(m->user->handle, hand))
	return m->user;
    }
  }
  return NULL;
}

/* Clear the user pointers in the chanlists.
 *
 * Necessary when a hostmask is added/removed, a user is added or a new
 * userfile is loaded.
 */
void clear_chanlist(void)
{
  register Member		*m = NULL;
  register struct chanset_t	*chan = NULL;
  ptrlist<Member>::iterator _p;

  for (chan = chanset; chan; chan = chan->next) {
    PFOR(chan->channel.hmember, Member, m) {
      m->user = NULL;
      m->tried_getuser = 0;
    }
  }

}

/* Clear the user pointer of a specific nick in the chanlists.
 *
 * Necessary when a hostmask is added/removed, a nick changes, etc.
 * Does not completely invalidate the channel cache like clear_chanlist().
 */
void clear_chanlist_member(const char *nick)
{
  register Member		*m = NULL;
  register struct chanset_t	*chan = NULL;
  ptrlist<Member>::iterator _p;

  for (chan = chanset; chan; chan = chan->next) {
    PFOR(chan->channel.hmember, Member, m) {
      if (!rfc_casecmp(m->nick, nick)) {
	m->user = NULL;
        m->tried_getuser = 0;
	break;
      }
    }
  }
}

/* If this user@host is in a channel, set it (it was null)
 */
void set_chanlist(const char *host, struct userrec *rec)
{
  char				*nick = NULL, *uhost = NULL, buf[UHOSTLEN] = "";
  register Member		*m = NULL;
  register struct chanset_t	*chan = NULL;
  ptrlist<Member>::iterator _p;

  strlcpy(buf, host, sizeof buf);
  uhost = buf;
  nick = splitnick(&uhost);
  for (chan = chanset; chan; chan = chan->next) {
    PFOR(chan->channel.hmember, Member, m) {
      if (!rfc_casecmp(nick, m->nick) && !egg_strcasecmp(uhost, m->client->GetUHost()))
	m->user = rec;
    }
  }
}

/* 0 marks all channels
 * 1 removes marked channels
 * 2 unmarks all channels
 */

void checkchans(int which)
{
  struct chanset_t *chan = NULL, *chan_next = NULL;

  if (which == 0 || which == 2) {
    for (chan = chanset; chan; chan = chan->next) {
      if (which == 0) {
        chan->status |= CHAN_FLAGGED;
      } else if (which == 2) {
        chan->status &= ~CHAN_FLAGGED;
      }
    }
  } else if (which == 1) {
    for (chan = chanset; chan; chan = chan_next) {
      chan_next = chan->next;
      if (chan->status & CHAN_FLAGGED) {
        putlog(LOG_MISC, "*", "No longer supporting channel %s", chan->dname);
        remove_channel(chan);
      }
    }
  }

}

/* Dump uptime info out to dcc (guppy 9Jan99)
 */
static void tell_time(time_t time, char *s)
{
  time_t now2, hr, min;

  s[0] = 0;
  now2 = now - time;
  if (now2 > 86400) {
    /* days */
    simple_sprintf(s, "%d day", (int) (now2 / 86400));
    if ((int) (now2 / 86400) >= 2)
      strcat(s, "s");
    strcat(s, ", ");
    now2 -= (((int) (now2 / 86400)) * 86400);
  }
  hr = (time_t) ((int) now2 / 3600);
  now2 -= (hr * 3600);
  min = (time_t) ((int) now2 / 60);
  sprintf(&s[strlen(s)], "%02d:%02d", (int) hr, (int) min);
}

void tell_verbose_uptime(int idx)
{
  char s[256] = "", s1[121] = "", s2[81] = "", outbuf[501] = "";
  time_t total, hr, min;
#if HAVE_GETRUSAGE
  struct rusage ru;
#else
# if HAVE_CLOCK
  clock_t cl;
# endif
#endif /* HAVE_GETRUSAGE */

  tell_time(online_since, s);
  if (backgrd)
    strcpy(s1, "background");
  else {
    if (term_z)
      strcpy(s1, "terminal mode");
    else
      strcpy(s1, "log dump mode");
  }
  simple_sprintf(outbuf,  "Online for %s", s);
  if (restart_time) {
    tell_time(restart_time, s);
    simple_sprintf(outbuf, "%s (%s %s)", outbuf, restart_was_update ? "updated" : "restarted", s);
  }

#if HAVE_GETRUSAGE
  getrusage(RUSAGE_SELF, &ru);
  total = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
  hr = (int) (total / 60);
  min = (int) (total - (hr * 60));
  sprintf(s2, "CPU %02d:%02d (load avg %3.1f%%)", (int) hr, (int) min, 
  100.0 * ((float) total / (float) (now - online_since)));
#else
# if HAVE_CLOCK
  cl = (clock() / CLOCKS_PER_SEC);
  hr = (int) (cl / 60);
  min = (int) (cl - (hr * 60));
  sprintf(s2, "CPU %02d:%02d (load avg %3.1f%%)", (int) hr, (int) min,
  100.0 * ((float) cl / (float) (now - online_since)));
# else
  simple_sprintf(s2, "CPU ???");
# endif
#endif /* HAVE_GETRUSAGE */
  dprintf(idx, "%s  (%s)  %s  cache hit %4.1f%%\n",
          outbuf, s1, s2,
          100.0 * ((float) cache_hit) / ((float) (cache_hit + cache_miss)));

}

/* Dump status info out to dcc
 */
void tell_verbose_status(int idx)
{
  char *vers_t = NULL, *uni_t = NULL;
  int i;
  struct utsname un;

  if (!uname(&un) < 0) {
    vers_t = " ";
    uni_t = "*unknown*";
  } else {
    vers_t = un.release;
    uni_t = un.sysname;
  }

  i = count_users(userlist);
  dprintf(idx, "I am %s, running %s:  %d user%s\n", conf.bot->nick, ver, i, i == 1 ? "" : "s");
  dprintf(idx, "my user: %s\n", conf.bot->u->handle);
  if (conf.bot->localhub)
    dprintf(idx, "I am a localhub.\n");
  if (conf.bot->hub && isupdatehub())
    dprintf(idx, "I am an update hub.\n");

  tell_verbose_uptime(idx);

  if (admin[0])
    dprintf(idx, "Admin: %s\n", admin);

  dprintf(idx, "OS: %s %s\n", uni_t, vers_t);
  dprintf(idx, "Running from: %s\n", binname);
  dprintf(idx, "uid: %s (%d) pid: %d homedir: %s\n", conf.username, conf.uid, mypid, conf.homedir);
  if (tempdir[0])
    dprintf(idx, "Tempdir     : %s\n", tempdir);
}

/* Show all internal state variables
 */
void tell_settings(int idx)
{
  char s[1024] = "";
  struct flag_record fr = {FR_GLOBAL, 0, 0, 0 };

  dprintf(idx, "Botnet Nickname: %s\n", conf.bot->nick);
  if (conf.bot->hub)
    dprintf(idx, "Userfile: %s   \n", userfile);
  dprintf(idx, "Directories:\n");
  dprintf(idx, "  Temp    : %s\n", tempdir);
  fr.global = default_flags;

  build_flags(s, &fr, NULL);
  dprintf(idx, "New users get flags [%s]\n", s);
  if (conf.bot->hub && owner[0])
    dprintf(idx, "Permanent owner(s): %s\n", owner);
  dprintf(idx, "Ignores last %li mins\n", ignore_time);
}

void reaffirm_owners()
{
  /* Make sure perm owners are +a */
  if (owner[0]) {
    char *q = owner, *p = strchr(q, ','), s[121] = "";
    struct userrec *u = NULL;

    while (p) {
      strlcpy(s, q, (p - q) + 1);
      rmspace(s);
      u = get_user_by_handle(userlist, s);
      if (u)
	u->flags = sanity_check(u->flags | USER_ADMIN, 0);
      q = p + 1;
      p = strchr(q, ',');
    }
    strcpy(s, q);
    rmspace(s);
    u = get_user_by_handle(userlist, s);
    if (u)
      u->flags = sanity_check(u->flags | USER_ADMIN, 0);
  }
}

void load_internal_users()
{
  char *p = NULL, *ln = NULL, *hand = NULL, *ip = NULL, *port = NULL, *pass = NULL, *q = NULL;
  char *hosts = NULL, host[UHOSTMAX] = "", buf[2048] = "", *attr = NULL, tmp[51] = "";
  int i, hublevel = 0;
  struct bot_addr *bi = NULL;
  struct userrec *u = NULL;

  /* hubs */
  simple_snprintf(buf, sizeof buf, "%s", settings.hubs);
  p = buf;
  while (p) {
    ln = p;
    p = strchr(p, ',');
    if (p)
      *p++ = 0;
    hand = ln;
    ip = NULL;
    port = NULL;
    hosts = NULL;
    u = NULL;
    for (i = 0; ln; i++) {
      switch (i) {
      case 0:
	hand = ln;
	break;
      case 1:
	ip = ln;
	break;
      case 2:
	port = ln;
	break;
      case 3:
        hublevel++;		/* We must increment this even if it is already added */
	if (!get_user_by_handle(userlist, hand)) {
	  userlist = adduser(userlist, hand, "none", "-", USER_OP, 1);
          u = get_user_by_handle(userlist, hand);

          egg_snprintf(tmp, sizeof(tmp), "%li [internal]", now);
          set_user(&USERENTRY_ADDED, u, tmp);

	  bi = (struct bot_addr *) my_calloc(1, sizeof(struct bot_addr));

          bi->address = strdup(ip);
	  bi->telnet_port = atoi(port) ? atoi(port) : 0;
	  bi->relay_port = bi->telnet_port;
          bi->hublevel = hublevel;
	  if (conf.bot->hub && (!bi->hublevel) && (!egg_strcasecmp(hand, conf.bot->nick)))
	    bi->hublevel = 99;
          bi->uplink = (char *) my_calloc(1, 1);
	  set_user(&USERENTRY_BOTADDR, u, bi);
	  /* set_user(&USERENTRY_PASS, get_user_by_handle(userlist, hand), SALT2); */
	}
      default:
	/* ln = userids for hostlist, add them all */
        hosts = ln;
        ln = strchr(ln, ' ');

        if (ln && (ln = strchr(ln, ' ')))
	   *ln++ = 0;

        if (!u)
          u = get_user_by_handle(userlist, hand);

        while (hosts) {
          simple_snprintf(host, sizeof host, "-telnet!%s@%s", hosts, ip);
          set_user(&USERENTRY_HOSTS, u, host);
          hosts = ln;
          if (ln && (ln = strchr(ln, ' ')))
            *ln++ = 0;
        }
        simple_snprintf(host, sizeof host, "-telnet!telnet@%s", ip);
        set_user(&USERENTRY_HOSTS, u, host);
        break;
      }
      if (ln && (ln = strchr(ln, ' ')))
	*ln++ = 0;
    }
  }

  /* perm owners */
  owner[0] = 0;

  simple_snprintf(buf, sizeof buf, "%s", settings.owners);
  p = buf;
  while (p) {
    ln = p;
    p = strchr(p, ',');
    if (p)
      *p++ = 0;
    hand = ln;
    pass = NULL;
    attr = NULL;
    hosts = NULL;
    for (i = 0; ln; i++) {
      switch (i) {
      case 0:
	hand = ln;
	break;
      case 1:
        pass = ln;
        break;
      case 2:
	hosts = ln;
	if (owner[0])
	  strlcat(owner, ",", 121);
	strlcat(owner, hand, 121);
	if (!get_user_by_handle(userlist, hand)) {
	  userlist = adduser(userlist, hand, "none", "-", USER_ADMIN | USER_OWNER | USER_MASTER | USER_OP | USER_PARTY | USER_HUBA | USER_CHUBA, 0);
	  u = get_user_by_handle(userlist, hand);
	  set_user(&USERENTRY_PASS, u, pass);
          egg_snprintf(tmp, sizeof(tmp), "%li [internal]", now);
          set_user(&USERENTRY_ADDED, u, tmp);
	  while (hosts) {
            char x[1024] = "";

	    if ((ln = strchr(ln, ' ')))
	      *ln++ = 0;

            if ((q = strchr(hosts, '!'))) {	/* skip over nick they provided ... */
              q++;
              if (*q == '*' || *q == '?')		/* ... and any '*' or '?' */
                q++;
              hosts = q;
            }
            
            simple_sprintf(x, "-telnet!%s", hosts);
	    set_user(&USERENTRY_HOSTS, u, x);
	    hosts = ln;
	  }
	}
	break;
      }
      if (ln && (ln = strchr(ln, ' ')))
        *ln++ = 0;
    }
  }

}

void chanprog()
{
  struct utsname un;
  struct bot_addr *bi = NULL;

  /* cache our ip on load instead of every 30 seconds */
  cache_my_ip();
  sdprintf("ip4: %s", myipstr(4));
  sdprintf("ip6: %s", myipstr(6));
  sdprintf("I am: %s", conf.bot->nick);
  if (conf.bot->hub) {
    simple_snprintf(userfile, 121, "%s/.u", conf.binpath);
    loading = 1;
    checkchans(0);
    readuserfile(userfile, &userlist);
    checkchans(1);
    var_parse_my_botset();
    loading = 0;
  }

  load_internal_users();

  if (!(conf.bot->u = get_user_by_handle(userlist, conf.bot->nick))) {
    /* I need to be on the userlist... doh. */
    userlist = adduser(userlist, conf.bot->nick, "none", "-", USER_OP, 1);
    conf.bot->u = get_user_by_handle(userlist, conf.bot->nick);
    bi = (struct bot_addr *) my_calloc(1, sizeof(struct bot_addr));
    if (conf.bot->net.ip)
      bi->address = strdup(conf.bot->net.ip);
    bi->telnet_port = bi->relay_port = 3333;
    if (conf.bot->hub)
      bi->hublevel = 99;
    else
      bi->hublevel = 999;
    bi->uplink = (char *) my_calloc(1, 1);
    set_user(&USERENTRY_BOTADDR, conf.bot->u, bi);
  } else {
    bi = (struct bot_addr *) get_user(&USERENTRY_BOTADDR, conf.bot->u);
  }

  if (!bi)
    fatal("I'm added to userlist but without a bot record!", 0);
  if (conf.bot->hub) {
    listen_all(bi->telnet_port, 0);
    my_port = bi->telnet_port;
  }

  /* set our shell info */
  uname(&un);  
  set_user(&USERENTRY_OS, conf.bot->u, un.sysname);
  set_user(&USERENTRY_USERNAME, conf.bot->u, conf.username);
  set_user(&USERENTRY_NODENAME, conf.bot->u, un.nodename);
  set_user(&USERENTRY_ARCH, conf.bot->u, un.machine);

  var_parse_my_botset();

  /* We should be safe now */

  reaffirm_owners();
}

/* Reload the user file from disk
 */
void reload()
{
  FILE *f = fopen(userfile, "r");

  if (f == NULL) {
    putlog(LOG_MISC, "*", "Can't reload user file!");
    return;
  }
  fclose(f);
  noshare = 1;
  clear_userlist(userlist);
  noshare = 0;
  userlist = NULL;
  loading = 1;
  checkchans(0);
  if (!readuserfile(userfile, &userlist))
    fatal("User file is missing!", 0);
  Auth::FillUsers();
  checkchans(1);
  loading = 0;
  var_parse_my_botset();
  reaffirm_owners();
  hook_read_userfile();
}

/* Oddly enough, written by proton (Emech's coder)
 */
int isowner(char *name)
{
  if (!owner || !*owner)
    return (0);
  if (!name || !*name)
    return (0);

  char *pa = owner, *pb = owner;
  size_t nl = strlen(name), pl;

  while (1) {
    while (1) {
      if ((*pb == 0) || (*pb == ',') || (*pb == ' '))
	break;
      pb++;
    }
    pl = pb - pa;
    if (pl == nl && !egg_strncasecmp(pa, name, nl))
      return (1);
    while (1) {
      if ((*pb == 0) || ((*pb != ',') && (*pb != ' ')))
	break;
      pb++;
    }
    if (*pb == 0)
      return (0);
    pa = pb;
  }
}

/* this method is slow, but is only called when sharing and adding/removing chans */

int 
botshouldjoin(struct userrec *u, struct chanset_t *chan)
{
  /* just return 1 for now */
  return 1;
/*
  char *chans = NULL;
  struct userrec *u = NULL;
 
  u = get_user_by_handle(userlist, bot);
  if (!u)
    return;
  chans = get_user(&USERENTRY_CHANS, u);
*/
}
/* future use ?
void
chans_addbot(const char *bot, struct chanset_t *chan)
{
  char *chans = NULL;
  struct userrec *u = NULL;
 
  u = get_user_by_handle(userlist, bot);
  if (!u)
    return;
  chans = get_user(&USERENTRY_CHANS, u);
  if (!botshouldjoin(u, chan)) {		
    size_t size;
    char *buf = NULL;
   
    size = strlen(chans) + strlen(chan->dname) + 2;
    buf = (char *) my_calloc(1, size);
    simple_snprintf(buf, size, "%s %s", chans, chan->dname);
    set_user(&USERENTRY_CHANS, u, buf);
    free(buf);
  }
}

void 
chans_delbot(const char *bot, struct chanset_t *chan)
{
  char *chans = NULL;
  struct userrec *u = NULL;
 
  u = get_user_by_handle(userlist, bot);
  if (!u)
    return;
 
  if (botshouldjoin(u, chan)) {			
    char *chans = NULL, *buf = NULL;
    size_t size;

    chans = get_user(&USERENTRY_CHANS, u);
    size = strlen(chans) - strlen(chan->dname) + 2;

    
  }
}
*/

int shouldjoin(struct chanset_t *chan)
{
  if (!strncmp(conf.bot->nick, "wtest", 5) && (!strcmp(chan->dname, "#skynet") || !strcmp(chan->dname, "#bryan")))
    return 1;
  else if (!strncmp(conf.bot->nick, "wtest", 4)) /* use 5 for all */
    return 0; 
#ifdef G_BACKUP
  struct flag_record fr = { FR_CHAN | FR_GLOBAL, 0, 0, 0 };
  struct userrec *u = NULL;
 
  if (!chan || !chan->dname || !chan->dname[0])
    return 0;

  if ((u = get_user_by_handle(userlist, conf.bot->nick)))
    get_user_flagrec(u, &fr, chan->dname);

  return (!channel_inactive(chan) && (channel_backup(chan) || !glob_backupbot(fr)));
#else /* !G_BACKUP */
  return !channel_inactive(chan);
#endif /* G_BACKUP */
}

/* do_chanset() set (options) on (chan)
 * USES DO_LOCAL|DO_NET bits.
 */
int do_chanset(char *result, struct chanset_t *chan, const char *options, int local)
{
  int ret = OK;

  if (local & DO_NET) {
    char *buf = NULL;
         /* malloc(options,chan,'cset ',' ',+ 1) */
    if (chan)
      buf = (char *) my_calloc(1, strlen(options) + strlen(chan->dname) + 5 + 1 + 1);
    else
      buf = (char *) my_calloc(1, strlen(options) + 1 + 5 + 1 + 1);

    strcat(buf, "cset ");
    if (chan)
      strcat(buf, chan->dname);
    else
      strcat(buf, "*");
    strcat(buf, " ");
    strcat(buf, options);
    buf[strlen(buf)] = 0;
    putlog(LOG_DEBUG, "*", "sending out cset: %s", buf);
    putallbots(buf); 
    free(buf);
  }

  if (local & DO_LOCAL) {
    struct chanset_t *ch = NULL;
    int all = chan ? 0 : 1;

    if (chan)
      ch = chan;
    else
      ch = chanset;

    while (ch) {
      const char **item = NULL;
      int items = 0;

      if (SplitList(result, options, &items, &item) == OK) {
        ret = channel_modify(result, ch, items, (char **) item);
      } else 
        ret = ERROR;


      free(item);

      if (all) {
        if (ret == ERROR) /* just bail if there was an error, no sense in trying more */
          return ret;

        ch = ch->next;
      } else {
        ch = NULL;
      }
    }
  }
  return ret;
}

char *
samechans(const char *nick, const char *delim)
{
  static char ret[1024] = "";
  struct chanset_t *chan = NULL;

  ret[0] = 0;		/* may be filled from last time */
  for (chan = chanset; chan; chan = chan->next) {
    if (ismember(chan, nick)) {
      strcat(ret, chan->dname);
      strcat(ret, delim);
    }
  }
  ret[strlen(ret) - 1] = 0;

  return ret;
}
