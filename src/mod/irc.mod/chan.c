/*
 *
 * chan.c -- part of irc.mod
 *   almost everything to do with channel manipulation
 *   telling channel status
 *   'who' response
 *   user kickban, kick, op, deop
 *   idle kicking
 *
 */


static time_t last_ctcp = (time_t) 0L;
static int    count_ctcp = 0;
static time_t last_invtime = (time_t) 0L;
static char   last_invchan[300] = "";

typedef struct resolvstruct {
  struct chanset_t *chan;
  char *nick;
} resolv_member;

static void resolv_member_callback(int id, void *client_data, const char *host, char **ips)
{
  resolv_member *r = (resolv_member *) client_data;

  if (!r || !r->chan || !r->nick)
    return;

  Member *m = NULL;
  char *ps = NULL, *pe = NULL, s[UHOSTLEN + 1];

  if (ips && ips[0]) {
    ptrlist<Member>::iterator _p;

    PFOR(r->chan->channel.hmember, Member, m) {
      if (!rfc_casecmp(m->nick, r->nick)) {
        if (!m->client->GetUIP()[0] && m->client->GetUHost()[0]) {
          ps = m->client->GetUHost();
          pe = strchr(ps, '@');
          if (pe) {
            char user[15] = "";

            simple_snprintf(user, pe - ps + 1, m->client->GetUHost());
            m->client->SetUIP(ips[0], user);
            if (!m->user) {
              simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUIP());
              m->user = get_user_by_host(s);
            }
            return;
          }
        }
      }
    }
  }

  return;
}


void resolve_to_member(struct chanset_t *chan, char *nick, char *host)
{
  resolv_member *r = (resolv_member *) my_calloc(1, sizeof(resolv_member));

  r->chan = chan;
  r->nick = strdup(nick);

  egg_dns_lookup(host, 20, resolv_member_callback, (void *) r);
}

/* ID length for !channels.
 */
#define CHANNEL_ID_LEN 5

/* Returns a pointer to a new channel member structure.
 */
static Member *newmember(struct chanset_t *chan, char *nick)
{
  Member *m = new Member(chan, nick);

  return m;
}

/* Always pass the channel dname (display name) to this function <cybah>
 */
static void update_idle(char *chname, char *nick)
{
  struct chanset_t *chan = findchan_by_dname(chname);

  if (chan) {
    Member::UpdateIdle(chan, nick);
  }
}

/* Returns the current channel mode.
 */
static char *getchanmode(struct chanset_t *chan)
{
  static char s[121] = "";
  int atr = chan->channel.mode;
  size_t i = 1;

  s[0] = '+';
  if (atr & CHANINV)
    s[i++] = 'i';
  if (atr & CHANPRIV)
    s[i++] = 'p';
  if (atr & CHANSEC)
    s[i++] = 's';
  if (atr & CHANMODER)
    s[i++] = 'm';
  if (atr & CHANNOCLR)
    s[i++] = 'c';
  if (atr & CHANNOCTCP)
    s[i++] = 'C';
  if (atr & CHANREGON)
    s[i++] = 'R';
  if (atr & CHANTOPIC)
    s[i++] = 't';
  if (atr & CHANMODR)
    s[i++] = 'M';
  if (atr & CHANLONLY)
    s[i++] = 'r';
  if (atr & CHANNOMSG)
    s[i++] = 'n';
  if (atr & CHANANON)
    s[i++] = 'a';
  if (atr & CHANKEY)
    s[i++] = 'k';
  if (chan->channel.maxmembers != 0)
    s[i++] = 'l';
  s[i] = 0;
  if (chan->channel.key[0])
    i += simple_snprintf(s + i, sizeof(s) - i, " %s", chan->channel.key);
  if (chan->channel.maxmembers != 0)
    simple_snprintf(s + i, sizeof(s) - i, " %d", chan->channel.maxmembers);
  return s;
}

static void check_exemptlist(struct chanset_t *chan, char *from)
{
  if (!use_exempts)
    return;

  bool ok = 0;

  for (masklist *e = chan->channel.exempt; e->mask[0]; e = e->next)
    if (wild_match(e->mask, from)) {
      add_mode(chan, '-', 'e', e->mask);
      ok = 1;
    }
  if (prevent_mixing && ok)
    flush_mode(chan, QUICK);
}

void priority_do(struct chanset_t * chan, bool opsonly, int action) 
{
  if (!me_op(chan))
    return;
  if (channel_pending(chan) || !shouldjoin(chan) || !channel_active(chan))
    return;

  Member *m = NULL;
  int ops = 0, targets = 0, bpos = 0, tpos = 0, ft = 0, ct = 0, actions = 0, sent = 0;
  ptrlist<Member>::iterator _p;

  PFOR(chan->channel.hmember, Member, m) {
    if (!m->user && !m->tried_getuser) {
      char s[256] = "";

      simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());
      m->user = get_user_by_host(s);
      if (!m->user && doresolv(chan) && m->client->GetUIP()[0]) {
        simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUIP());
        m->user = get_user_by_host(s);
      }
      m->tried_getuser = 1;
    }

    if (m->user && m->user->bot && (m->user->flags & USER_OP)) {
      ops++;
      if (!strcmp(m->nick, botname))
        bpos = (ops - 1);

    } else if (!opsonly || chan_hasop(m)) {
        struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
        if (m->user)
          get_user_flagrec(m->user, &fr, chan->dname, chan);

        if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) || /* +d */
           ((!channel_privchan(chan) && !chan_op(fr) && !glob_op(fr)) || /* simply no +o flag. */
           (channel_privchan(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) { /* private? */
          targets++;
        }
    }
  }

  if (!targets || !ops)
    return;

  ft = (bpos * targets) / ops;
  ct = ((bpos + 2) * targets + (ops - 1)) / ops;
  ct = (ct - ft + 1);
  if (ct > 20)
    ct = 20;
  while (ft >= targets)
    ft -= targets;
  actions = 0;
  sent = 0;
  PFOR (chan->channel.hmember, Member, m) {
    if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname, chan);
 
      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
          ((!channel_privchan(chan) && !chan_op(fr) && !glob_op(fr)) ||
           (channel_privchan(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) {

        if (tpos >= ft) {
          if ((action == PRIO_DEOP) && !chan_sentdeop(m)) {
            actions++;
            sent++;
            add_mode(chan, '-', 'o', m->nick);
            if (actions >= ct) {
              flush_mode(chan, QUICK);
              return;
            }
          } else if ((action == PRIO_KICK) && !chan_sentkick(m)) {
            actions++;
            sent++;
            if (chan->closed_ban)
              do_closed_kick(chan, m);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_CLOSED));
            m->flags |= SENTKICK;
            if (actions >= ct)
              return;
          }
        }
        tpos++;
      
      }
    }
  }

  ct = ct - actions;
  if (ct > ft)
    ct = ft;
  ft = 0;
  actions = 0;
  tpos = 0;
  PFOR (chan->channel.hmember, Member, m) {
    if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname, chan);

      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
          ((!channel_privchan(chan) && !chan_op(fr) && !glob_op(fr)) || 
           (channel_privchan(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) {

        if (tpos >= ft) {
          if ((action == PRIO_DEOP) && !chan_sentdeop(m)) {
            actions++;
            sent++;
            add_mode(chan, '-', 'o', m->nick);
            if ((actions >= ct) || (sent > 20)) {
              flush_mode(chan, QUICK);
              return;
            }
          } else if ((action == PRIO_KICK) && !chan_sentkick(m)) {
            actions++;
            if (chan->closed_ban)
              do_closed_kick(chan, m);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_CLOSED));
            m->flags |= SENTKICK;
            if ((actions >= ct) || (sent > 5))
              return;
          }
        }
        tpos++;
        
      }
    }
  }
}

/* lame code 
static int target_priority(struct chanset_t * chan, memberlist *target, int opsonly) 
{
  memberlist *m;
  int ops = 0, targets = 0, bpos = 0, ft = 0, ct = 0, tp = (-1), pos = 0;

  return 1;
  for (m = chan->channel.member; m && m->nick[0]; m = m->next) {
    if (m->user && ((m->user->flags & (USER_BOT | USER_OP)) == (USER_BOT | USER_OP))) {
      ops++;
      if (match_my_nick(m->nick))
        bpos = ops;
    } else if (!opsonly || chan_hasop(m)) {
      struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };

      if (m->user)
        get_user_flagrec(m->user, &fr, chan->dname);

      if (((glob_deop(fr) && !chan_op(fr)) || chan_deop(fr)) ||
         ((!channel_privchan(chan) && !chan_op(fr) && !glob_op(fr)) || 
         (channel_privchan(chan) && !glob_bot(fr) && !glob_owner(fr) && !chan_op(fr)))) { 
        targets++;
      }
    }
    if (m == target)
      tp = pos;
    pos++;
  }
  if (!targets || !ops || (tp < 0)) {
    return 0;
  }
  ft = (bpos * targets) / ops;
  ct = ((bpos + 2) * targets + (ops - 1)) / ops;
  ct = (ct - ft + 1);
  if (ct > 20)
    ct = 20;
  while (ft >= targets) {
    ft -= targets;
  }
  if (ct >= targets) {
    putlog(LOG_MISC, "*", "%s 1 ct >= targets; ct %d targets %d", target, ct, targets);
    if ((tp >= ft) || (tp <= (ct % targets))) {
      putlog(LOG_MISC, "*", "%s (1) first if, tp %d ft %d ct/targets %d", target, tp, ft, (ct % targets));
      return 1;
    }
  } else {
    putlog(LOG_MISC, "*", "%s 2 else, ct %d targets %d", target, ct, targets);
    if ((tp >= ft) && (tp <= ct)) {
      putlog(LOG_MISC, "*", "%s (1) second if, tp %d ft %d", target, tp, ft);
      return 1;
    }
  }
  putlog(LOG_MISC, "*", "%s (0) returning 0", target);
  return 0;
}
*/

/* Check a channel and clean-out any more-specific matching masks.
 *
 * Moved all do_ban(), do_exempt() and do_invite() into this single function
 * as the code bloat is starting to get rediculous <cybah>
 */
static void do_mask(struct chanset_t *chan, masklist *m, char *mask, char Mode)
{
  for (; m && m->mask[0]; m = m->next)
    if (wild_match(mask, m->mask) && rfc_casecmp(mask, m->mask))
      add_mode(chan, '-', Mode, m->mask);
  add_mode(chan, '+', Mode, mask);
  flush_mode(chan, QUICK);
}

/* This is a clone of detect_flood, but works for channel specificity now
 * and handles kick & deop as well.
 */
static bool detect_chan_flood(char *floodnick, char *floodhost, char *from,
			     struct chanset_t *chan, int which, char *victim)
{
  if (!chan || (which < 0) || (which >= FLOOD_CHAN_MAX))
    return 0;

  /* Okay, make sure i'm not flood-checking myself */
  if (match_my_nick(floodnick))
    return 0;
  if (!egg_strcasecmp(floodhost, botuserhost))
    return 0;
  /* My user@host (?) */

  /* Do not punish non-existant channel members and IRC services like
   * ChanServ
   */
  Member *m = ismember(chan, floodnick);

  if (!m && (which != FLOOD_JOIN) && (which != FLOOD_PART))
    return 0;

  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  get_user_flagrec(get_user_by_host(from), &fr, chan->dname, chan);
  if (glob_bot(fr) ||
      ((which == FLOOD_DEOP) &&
       (glob_master(fr) || chan_master(fr))) ||
      ((which == FLOOD_KICK) &&
       (glob_master(fr) || chan_master(fr))) ||
      ((which != FLOOD_DEOP) && (which != FLOOD_KICK) && 
       ( (chk_noflood(fr) || 
         (m && chan->flood_exempt_mode == FLOOD_EXEMPT_OP && chan_hasop(m)) || 
         (m && chan->flood_exempt_mode == FLOOD_EXEMPT_VOICE && chan_hasvoice(m)) )
      )))
    return 0;

  char h[UHOSTLEN] = "", ftype[12] = "", *p = NULL;
  struct userrec *u = NULL;
  int thr = 0;
  time_t lapse = 0;

  /* Determine how many are necessary to make a flood. */
  switch (which) {
  case FLOOD_PRIVMSG:
  case FLOOD_NOTICE:
    thr = chan->flood_pub_thr;
    lapse = chan->flood_pub_time;
    strcpy(ftype, "pub");
    break;
  case FLOOD_CTCP:
    thr = chan->flood_ctcp_thr;
    lapse = chan->flood_ctcp_time;
    strcpy(ftype, "pub");
    break;
  case FLOOD_NICK:
    thr = chan->flood_nick_thr;
    lapse = chan->flood_nick_time;
    strcpy(ftype, "nick");
    break;
  case FLOOD_JOIN:
  case FLOOD_PART:
    thr = chan->flood_join_thr;
    lapse = chan->flood_join_time;
      strcpy(ftype, "join");
    break;
  case FLOOD_DEOP:
    thr = chan->flood_deop_thr;
    lapse = chan->flood_deop_time;
    strcpy(ftype, "deop");
    break;
  case FLOOD_KICK:
    thr = chan->flood_kick_thr;
    lapse = chan->flood_kick_time;
    strcpy(ftype, "kick");
    break;
  }
  if ((thr == 0) || (lapse == 0))
    return 0;			/* no flood protection */

  if ((which == FLOOD_KICK) || (which == FLOOD_DEOP))
    p = floodnick;
  else {
    p = strchr(floodhost, '@');
    if (p) {
      p++;
    }
    if (!p)
      return 0;
  }
  if (rfc_casecmp(chan->floodwho[which], p)) {	/* new */
    strncpy(chan->floodwho[which], p, sizeof(chan->floodwho[which]));
    chan->floodwho[which][sizeof(chan->floodwho[which])] = 0;
    chan->floodtime[which] = now;
    chan->floodnum[which] = 1;
    return 0;
  }
  if (chan->floodtime[which] < now - lapse) {
    /* Flood timer expired, reset it */
    chan->floodtime[which] = now;
    chan->floodnum[which] = 1;
    return 0;
  }
  /* Deop'n the same person, sillyness ;) - so just ignore it */
  if (which == FLOOD_DEOP) {
    if (!rfc_casecmp(chan->deopd, victim))
      return 0;
    else
      strcpy(chan->deopd, victim);
  }
  chan->floodnum[which]++;
  if (chan->floodnum[which] >= thr) {	/* FLOOD */
    /* Reset counters */
    chan->floodnum[which] = 0;
    chan->floodtime[which] = 0;
    chan->floodwho[which][0] = 0;
    if (which == FLOOD_DEOP)
      chan->deopd[0] = 0;
    u = get_user_by_host(from);
    switch (which) {
    case FLOOD_PRIVMSG:
    case FLOOD_NOTICE:
    case FLOOD_CTCP:
      /* Flooding chan! either by public or notice */
      if (!chan_sentkick(m) && me_op(chan)) {
	putlog(LOG_MODES, chan->dname, "Channel flood from %s -- kicking", floodnick);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, floodnick, kickprefix, response(RES_FLOOD));
	m->flags |= SENTKICK;
      }
      return 1;
    case FLOOD_JOIN:
    case FLOOD_PART:
    case FLOOD_NICK:
      if (use_exempts &&
	  (u_match_mask(global_exempts, from) ||
	   u_match_mask(chan->exempts, from)))
	return 1;
      simple_snprintf(h, sizeof(h), "*!*@%s", p);
      if (!isbanned(chan, h) && me_op(chan)) {
	check_exemptlist(chan, from);
	do_mask(chan, chan->channel.ban, h, 'b');
      }
      if ((u_match_mask(global_bans, from))
	  || (u_match_mask(chan->bans, from)))
	return 1;		/* Already banned */
      if (which == FLOOD_JOIN || which == FLOOD_PART)
	putlog(LOG_MISC | LOG_JOIN, chan->dname, "JOIN flood from @%s!  Banning.", p);
      else
	putlog(LOG_MISC | LOG_JOIN, chan->dname, "NICK flood from @%s!  Banning.", p);
      strcpy(ftype + 4, " flood");
      u_addmask('b', chan, h, conf.bot->nick, ftype, now + (60 * chan->ban_time), 0);
      if (which == FLOOD_PART)
        add_mode(chan, '+', 'b', h);
      if (!channel_enforcebans(chan) && me_op(chan)) {
	  char s[UHOSTLEN] = "";
          ptrlist<Member>::iterator _p;

          PFOR (chan->channel.hmember, Member, m) {
	    simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());
	    if (!chan_sentkick(m) && wild_match(h, s) &&
		(m->joined >= chan->floodtime[which]) &&
		!match_my_nick(m->nick) && me_op(chan)) {
	      m->flags |= SENTKICK;
	      if (which == FLOOD_JOIN)
   	        dprintf(DP_SERVER, "KICK %s %s :%sjoin flood\n", chan->name, m->nick, kickprefix);
	      else
                dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, response(RES_NICKFLOOD));
	    }
	  }
	}
      return 1;
    case FLOOD_KICK:
      if (me_op(chan) && !chan_sentkick(m)) {
	putlog(LOG_MODES, chan->dname, "Kicking %s, for mass kick.", floodnick);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, floodnick, kickprefix, response(RES_KICKFLOOD));
	m->flags |= SENTKICK;
      }
    return 1;
    case FLOOD_DEOP:
      if (me_op(chan) && !chan_sentkick(m)) {
	putlog(LOG_MODES, chan->dname,
	       "Mass deop on %s by %s", chan->dname, from);
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, floodnick, kickprefix, response(RES_MASSDEOP));
	m->flags |= SENTKICK;
      }
      if (u) {
        char s[256] = "";

        simple_snprintf(s, sizeof(s), "Mass deop on %s by %s", chan->dname, from);
        deflag_user(u, DEFLAG_MDOP, s, chan);
      }
      return 1;
    }
  }
  return 0;
}

/* Given a chan/m do all necesary exempt checks and ban. */
static void refresh_ban_kick(struct chanset_t*, Member *, char *);
static void do_closed_kick(struct chanset_t *chan, Member *m)
{
  if (!chan || !m) return;

  char s[UHOSTLEN] = "", *s1 = NULL;

  simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());

  if (!(use_exempts &&
        (u_match_mask(global_exempts,s) ||
         u_match_mask(chan->exempts, s)))) {
    if (u_match_mask(global_bans, s) || u_match_mask(chan->bans, s))
      refresh_ban_kick(chan, m, s);

    check_exemptlist(chan, s);
    s1 = quickban(chan, m->client->GetUHost());
    u_addmask('b', chan, s1, conf.bot->nick, "joined closed chan", now + (60 * chan->ban_time), 0);
  }
  return;
}

/* Given a [nick!]user@host, place a quick ban on them on a chan.
 */
static char *quickban(struct chanset_t *chan, char *uhost)
{
  static char s1[512] = "";

  maskhost(uhost, s1);
  /* this used to only happen with strict_host == 0 */
  if (strlen(s1) != 1)
    s1[2] = '*';		/* arthur2 */
  do_mask(chan, chan->channel.ban, s1, 'b');
  return s1;
}

/* Kick any user (except friends/masters) with certain mask from channel
 * with a specified comment.  Ernst 18/3/1998
 */
static void kick_all(struct chanset_t *chan, char *hostmask, const char *comment, int bantype)
{
  int flushed = 0;
  char s[UHOSTLEN] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  Member *m = NULL;
  ptrlist<Member>::iterator _p;

  PFOR(chan->channel.hmember, Member, m) {
    simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());
    get_user_flagrec(m->user, &fr, chan->dname, chan);
    if ((wild_match(hostmask, s) || match_cidr(hostmask, s)) && 
        !chan_sentkick(m) &&
	!match_my_nick(m->nick) && !chan_issplit(m) &&
	!(use_exempts &&
	  ((bantype && (isexempted(chan, s) || (chan->ircnet_status & CHAN_ASKED_EXEMPTS))) ||
	   (u_match_mask(global_exempts, s) ||
	    u_match_mask(chan->exempts, s))))) {
      if (!flushed) {
	/* We need to kick someone, flush eventual bans first */
	flush_mode(chan, QUICK);
	flushed += 1;
      }
      if (!chan_sentkick(m)) {
        m->flags |= SENTKICK;	/* Mark as pending kick */
        dprintf(DP_MODE, "KICK %s %s :%s%s\n", chan->name, m->nick, kickprefix, comment);
      }
    }
  }
}

/* If any bans match this wildcard expression, refresh them on the channel.
 */
static void refresh_ban_kick(struct chanset_t* chan, Member *m, char *user)
{
  if (!m || chan_sentkick(m))
    return;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  get_user_flagrec(m->user, &fr, chan->dname, chan);

  /* Check global bans in first cycle and channel bans
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (register maskrec* b = cycle ? chan->bans : global_bans; b; b = b->next) {
      if (wild_match(b->mask, user) || match_cidr(b->mask, user)) {
        if (role == 1)
  	  add_mode(chan, '-', 'o', m->nick);	/* Guess it can't hurt.	*/
	check_exemptlist(chan, user);
	do_mask(chan, chan->channel.ban, b->mask, 'b');
	b->lastactive = now;
        if (role == 2) {
          char c[512] = "";		/* The ban comment.	*/

          if (b->desc && b->desc[0] != '@')
	    simple_snprintf(c, sizeof(c), "banned: %s", b->desc);
          kick_all(chan, b->mask, c[0] ? (const char *) c : "You are banned", 0);
        }
        return;					/* Drop out on 1st ban.	*/
      } 
    }
  }
}

/* This is a bit cumbersome at the moment, but it works... Any improvements
 * then feel free to have a go.. Jason
 */
static void refresh_exempt(struct chanset_t *chan, char *user)
{
  maskrec *e = NULL;
  masklist *b = NULL;

  /* Check global exempts in first cycle and channel exempts
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (e = cycle ? chan->exempts : global_exempts; e; e = e->next) {
      if (wild_match(user, e->mask) || wild_match(e->mask, user) || match_cidr(e->mask, user)) {
        for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
          if (wild_match(b->mask, user) || wild_match(user, b->mask)) {
            if (e->lastactive < now - 60 && !isexempted(chan, e->mask)) {
              do_mask(chan, chan->channel.exempt, e->mask, 'e');
              e->lastactive = now;
            }
          }
        }
      }
    }
  }
}

static void refresh_invite(struct chanset_t *chan, char *user)
{
  maskrec *i = NULL;

  /* Check global invites in first cycle and channel invites
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (i = cycle ? chan->invites : global_invites; i; i = i->next) {
      if (wild_match(i->mask, user) &&
	  ((i->flags & MASKREC_STICKY) || (chan->channel.mode & CHANINV))) {
        if (i->lastactive < now - 60 && !isinvited(chan, i->mask)) {
          do_mask(chan, chan->channel.invite, i->mask, 'I');
	  i->lastactive = now;
	  return;
	}
      }
    }
  }
}

/* Enforce all channel bans in a given channel.  Ernst 18/3/1998
 */
static void enforce_bans(struct chanset_t *chan)
{
  if (!me_op(chan))
    return;			/* Can't do it :( */

  if ((chan->ircnet_status & CHAN_ASKED_EXEMPTS))
    return;

  /* Go through all bans, kicking the users. */
  for (masklist *b = chan->channel.ban; b && b->mask[0]; b = b->next) {
    if (!(wild_match(b->mask, me) || match_cidr(b->mask, meip)) && !isexempted(chan, b->mask))
      kick_all(chan, b->mask, "You are banned", 1);
  }
}

/* Make sure that all who are 'banned' on the userlist are actually in fact
 * banned on the channel.
 *
 * Note: Since i was getting a ban list, i assume i'm chop.
 */
static void recheck_bans(struct chanset_t *chan)
{
  maskrec *u = NULL;

  /* Check global bans in first cycle and channel bans
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (u = cycle ? chan->bans : global_bans; u; u = u->next)
      if (!isbanned(chan, u->mask) && (!channel_dynamicbans(chan) || (u->flags & MASKREC_STICKY)))
	add_mode(chan, '+', 'b', u->mask);
  }
}

/* Make sure that all who are exempted on the userlist are actually in fact
 * exempted on the channel.
 *
 * Note: Since i was getting an excempt list, i assume i'm chop.
 */
static void recheck_exempts(struct chanset_t *chan)
{
  maskrec *e = NULL;
  masklist *b = NULL;

  /* Check global exempts in first cycle and channel exempts
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++) {
    for (e = cycle ? chan->exempts : global_exempts; e; e = e->next) {
      if (!isexempted(chan, e->mask) &&
          (!channel_dynamicexempts(chan) || (e->flags & MASKREC_STICKY)))
        add_mode(chan, '+', 'e', e->mask);
      for (b = chan->channel.ban; b && b->mask[0]; b = b->next) {
        if ((wild_match(b->mask, e->mask) || wild_match(e->mask, b->mask)) &&
            !isexempted(chan, e->mask))
	  add_mode(chan,'+','e',e->mask);
	/* do_mask(chan, chan->channel.exempt, e->mask, 'e');*/
      }
    }
  }
}

/* Make sure that all who are invited on the userlist are actually in fact
 * invited on the channel.
 *
 * Note: Since i was getting an invite list, i assume i'm chop.
 */

static void recheck_invites(struct chanset_t *chan)
{
  maskrec *ir = NULL;

  /* Check global invites in first cycle and channel invites
     in second cycle. */
  for (int cycle = 0; cycle < 2; cycle++)  {
    for (ir = cycle ? chan->invites : global_invites; ir; ir = ir->next) {
      /* If invite isn't set and (channel is not dynamic invites and not invite
       * only) or invite is sticky.
       */
      if (!isinvited(chan, ir->mask) && ((!channel_dynamicinvites(chan) &&
          !(chan->channel.mode & CHANINV)) || ir->flags & MASKREC_STICKY))
	add_mode(chan, '+', 'I', ir->mask);
	/* do_mask(chan, chan->channel.invite, ir->mask, 'I');*/
    }
  }
}

/* Resets the masks on the channel.
 */
static void resetmasks(struct chanset_t *chan, masklist *m, maskrec *mrec, maskrec *global_masks, char mode)
{
  if (!me_op(chan))
    return;                     /* Can't do it */

  /* Remove masks we didn't put there */
  for (; m && m->mask[0]; m = m->next) {
    if (!u_equals_mask(global_masks, m->mask) && !u_equals_mask(mrec, m->mask))
      add_mode(chan, '-', mode, m->mask);
  }

  /* Make sure the intended masks are still there */
  switch (mode) {
    case 'b':
      recheck_bans(chan);
      break;
    case 'e':
      recheck_exempts(chan);
      break;
    case 'I':
      recheck_invites(chan);
      break;
    default:
      putlog(LOG_MISC, "*", "(!) Invalid mode '%c' in resetmasks()", mode);
      break;
  }
}

void check_this_ban(struct chanset_t *chan, char *banmask, bool sticky)
{
  if (!me_op(chan))
    return;

  char user[UHOSTLEN] = "";
  Member *m = NULL;
  ptrlist<Member>::iterator _p;
  
  PFOR(chan->channel.hmember, Member, m) {
    simple_snprintf(user, sizeof(user), "%s!%s", m->nick, m->client->GetUHost());
    if (wild_match(banmask, user) &&
        !(use_exempts &&
          (u_match_mask(global_exempts, user) ||
           u_match_mask(chan->exempts, user))))
      refresh_ban_kick(chan, m, user);
  }
  if (!isbanned(chan, banmask) &&
      (!channel_dynamicbans(chan) || sticky))
    add_mode(chan, '+', 'b', banmask);
}

void check_this_exempt(struct chanset_t *chan, char *mask, bool sticky)
{
  if (!isexempted(chan, mask) && (!channel_dynamicexempts(chan) || sticky))
    add_mode(chan, '+', 'e', mask);
}

void check_this_invite(struct chanset_t *chan, char *mask, bool sticky)
{
  if (!isinvited(chan, mask) && (!channel_dynamicinvites(chan) || sticky))
    add_mode(chan, '+', 'I', mask);
}

void recheck_channel_modes(struct chanset_t *chan)
{
  int cur = chan->channel.mode,
      mns = chan->mode_mns_prot,
      pls = chan->mode_pls_prot;

  if (channel_closed(chan)) {
    if (chan->closed_invite) {
      pls |= CHANINV;
      mns &= ~CHANINV;
    }
    if (chan->closed_private) {
      pls |= CHANPRIV;
      mns &= ~CHANPRIV;
    }
  }

  if (!(chan->status & CHAN_ASKEDMODES)) {
    if (pls & CHANINV && !(cur & CHANINV))
      add_mode(chan, '+', 'i', "");
    else if (mns & CHANINV && cur & CHANINV)
      add_mode(chan, '-', 'i', "");
    if (pls & CHANPRIV && !(cur & CHANPRIV))
      add_mode(chan, '+', 'p', "");
    else if (mns & CHANPRIV && cur & CHANPRIV)
      add_mode(chan, '-', 'p', "");
    if (pls & CHANSEC && !(cur & CHANSEC))
      add_mode(chan, '+', 's', "");
    else if (mns & CHANSEC && cur & CHANSEC)
      add_mode(chan, '-', 's', "");
    if (pls & CHANMODER && !(cur & CHANMODER))
      add_mode(chan, '+', 'm', "");
    else if (mns & CHANMODER && cur & CHANMODER)
      add_mode(chan, '-', 'm', "");
    if (pls & CHANNOCLR && !(cur & CHANNOCLR))
      add_mode(chan, '+', 'c', "");
    else if (mns & CHANNOCLR && cur & CHANNOCLR)
      add_mode(chan, '-', 'c', "");
    if (pls & CHANNOCTCP && !(cur & CHANNOCTCP))
      add_mode(chan, '+', 'C', "");
    else if (mns & CHANNOCTCP && cur & CHANNOCTCP)
      add_mode(chan, '-', 'C', "");
    if (pls & CHANREGON && !(cur & CHANREGON))
      add_mode(chan, '+', 'R', "");
    else if (mns & CHANREGON && cur & CHANREGON)
      add_mode(chan, '-', 'R', "");
    if (pls & CHANMODR && !(cur & CHANMODR))
      add_mode(chan, '+', 'M', "");
    else if (mns & CHANMODR && cur & CHANMODR)
      add_mode(chan, '-', 'M', "");
    if (pls & CHANLONLY && !(cur & CHANLONLY))
      add_mode(chan, '+', 'r', "");
    else if (mns & CHANLONLY && cur & CHANLONLY)
      add_mode(chan, '-', 'r', "");
    if (pls & CHANTOPIC && !(cur & CHANTOPIC))
      add_mode(chan, '+', 't', "");
    else if (mns & CHANTOPIC && cur & CHANTOPIC)
      add_mode(chan, '-', 't', "");
    if (pls & CHANNOMSG && !(cur & CHANNOMSG))
      add_mode(chan, '+', 'n', "");
    else if ((mns & CHANNOMSG) && (cur & CHANNOMSG))
      add_mode(chan, '-', 'n', "");
    if ((pls & CHANANON) && !(cur & CHANANON))
      add_mode(chan, '+', 'a', "");
    else if ((mns & CHANANON) && (cur & CHANANON))
      add_mode(chan, '-', 'a', "");
    if ((pls & CHANQUIET) && !(cur & CHANQUIET))
      add_mode(chan, '+', 'q', "");
    else if ((mns & CHANQUIET) && (cur & CHANQUIET))
      add_mode(chan, '-', 'q', "");
    if ((chan->limit_prot != 0) && (chan->channel.maxmembers == 0)) {
      char s[11] = "";

      simple_snprintf(s, sizeof(s), "%d", chan->limit_prot);
      add_mode(chan, '+', 'l', s);
    } else if ((mns & CHANLIMIT) && (chan->channel.maxmembers != 0))
      add_mode(chan, '-', 'l', "");
    if (chan->key_prot[0]) {
      if (rfc_casecmp(chan->channel.key, chan->key_prot) != 0) {
        if (chan->channel.key[0])
	  add_mode(chan, '-', 'k', chan->channel.key);
        add_mode(chan, '+', 'k', chan->key_prot);
      }
    } else if ((mns & CHANKEY) && (chan->channel.key[0]))
      add_mode(chan, '-', 'k', chan->channel.key);
  }
}

static void check_this_member(struct chanset_t *chan, char *nick, struct flag_record *fr)
{
  if (match_my_nick(nick) || !me_op(chan))
    return;

  Member *m = ismember(chan, nick);
  if (!m)
    return;

  /* +d or bitch and not an op
   * we dont check private because +private does not imply bitch. */
  if (chan_hasop(m) && 
      (chk_deop(*fr, chan) ||
       (!loading && userlist && chan_bitch(chan) && !chk_op(*fr, chan)) ) ) {
    /* if (target_priority(chan, m, 1)) */
      add_mode(chan, '-', 'o', m->nick);
  } else if (!chan_hasop(m) && dovoice(chan) && m->user && !u_pass_match(m->user, "-") && chk_autoop(*fr, chan)) {
    do_op(m->nick, chan, 1, 0);
  }
  if (dovoice(chan)) {
    if (chan_hasvoice(m) && !chan_hasop(m)) {
      /* devoice +q users .. */
      if (chk_devoice(*fr))
        add_mode(chan, '-', 'v', m->nick);
    } else if (!chan_hasvoice(m) && !chan_hasop(m)) {
      /* voice +v users */
      if (chk_voice(*fr, chan)) {
        add_mode(chan, '+', 'v', m->nick);
        if (m->flags & EVOICE)
          m->flags &= ~EVOICE;
      }
    }
  }

  char s[UHOSTLEN] = "";

  simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());
  /* check vs invites */
  if (use_invites &&
      (u_match_mask(global_invites,s) ||
       u_match_mask(chan->invites, s)))
    refresh_invite(chan, s);
  /* don't kickban if permanent exempted */
  if (!(use_exempts &&
        (u_match_mask(global_exempts, s) ||
         u_match_mask(chan->exempts, s)))) {

    /* Are they banned in the internal list? */
    if (u_match_mask(global_bans, s) || u_match_mask(chan->bans, s))
      refresh_ban_kick(chan, m, s);

    /* are they +k ? */
    if (!chan_sentkick(m) && (chan_kick(*fr) || glob_kick(*fr)) && me_op(chan)) {
      char *p = (char *) get_user(&USERENTRY_COMMENT, m->user);

      check_exemptlist(chan, s);
      quickban(chan, m->client->GetUHost());
      dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, m->nick, bankickprefix, p ? p : response(RES_KICKBAN));
      m->flags |= SENTKICK;
    }
  }
}

void check_this_user(char *hand, int del, char *host)
{
  char s[UHOSTLEN] = "";
  Member *m = NULL;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  ptrlist<Member>::iterator _p;

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    PFOR(chan->channel.hmember, Member, m) {
      simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());
      u = m->user ? m->user : get_user_by_host(s);
      if ((u && !egg_strcasecmp(u->handle, hand) && del < 2) ||
	  (!u && del == 2 && wild_match(host, s))) {
	u = del ? NULL : u;
	get_user_flagrec(u, &fr, chan->dname, chan);
	check_this_member(chan, m->nick, &fr);
      }
    }
  }
}

static void enforce_bitch(struct chanset_t *chan) {
  if (!chan || !me_op(chan)) 
    return;
  priority_do(chan, 1, PRIO_DEOP);
}

void enforce_closed(struct chanset_t *chan) {
  if (!chan || !me_op(chan)) 
    return;

  char buf[3] = "", *p = buf;

  if (chan->closed_invite && !(chan->channel.mode & CHANINV) && !(chan->mode_mns_prot & CHANINV))
    *p++ = 'i';
  if (chan->closed_private && !(chan->channel.mode & CHANPRIV) && !(chan->mode_mns_prot & CHANPRIV))
    *p++ = 'p';
  *p = 0;
  if (buf && buf[0])
    dprintf(DP_MODE, "MODE %s +%s\n", chan->name, buf);
  priority_do(chan, 0, PRIO_KICK);
}

inline static char *
take_massopline(char *op, char **to_op)
{
  char *nicks = (char *) my_calloc(1, 151),
       *modes = (char *) my_calloc(1, 31),
       *nick = NULL;
  register bool useop = 0;
  static char ret[182] = "";

  egg_memset(ret, 0, sizeof(ret));
  for (register unsigned int i = 0; i < modesperline; i++) {
    if (*to_op[0] || op) {
      /* if 'op' then use it, then move on to to_op */
      if (!useop && op) {
        nick = op;
        useop = 1;
      } else if (*to_op[0])
        nick = newsplit(to_op);
      if (nick) {
        strcat(modes, "+o");
        strcat(nicks, nick);
        strcat(nicks, " "); 
      }
    }
  }
  
  strcat(ret, modes);
  strcat(ret, " ");
  strcat(ret, nicks);
  free(modes);
  free(nicks);
  
  return ret;
}

inline static char *
take_makeline(char *op, char *deops, unsigned int deopn, size_t deops_len)
{
  bool opn = op ? 1 : 0;
  unsigned int n = opn + deopn;		/* op + deops */
  unsigned int pos = randint(deopn), i;
  static char ret[151] = "";

  egg_memset(ret, 0, sizeof(ret));
  for (i = 0; i < n; i++) {
    if (opn && i == pos)
      strcat(ret, "+o");
    else if (deopn)
      strcat(ret, "-o");
  }

  strcat(ret, " ");

  for (i = 0; i < n; i++) {
    if (opn && i == pos)
      strcat(ret, op);
    else if (deopn)
      strcat(ret, newsplit(&deops));

    strcat(ret, " ");
  }
  return ret;  
}

static void
do_take(struct chanset_t *chan)
{
  char to_deop[2048] = "", *to_deop_ptr = to_deop;
  char to_op[2048] = "", *to_op_ptr = to_op;
  register Member *m = NULL;
  register size_t to_op_len = 0, to_deop_len = 0;
  ptrlist<Member>::iterator _p;

  /* Make lists of who needs to be opped, and who needs to be deopped */
  PFOR(chan->channel.hmember, Member, m) {
    if (rfc_casecmp(m->nick, botname)) {
      register const bool isbot = m->user && m->user->bot ? 1 : 0;

      /* Avoid countless unneeded operations from strcat/strlen */
      if (isbot && !(m->flags & CHANOP)) {
        to_op_len += strlcpy(to_op + to_op_len, m->nick, sizeof(to_op) - to_op_len);
        *(to_op + to_op_len++) = ' ';
      } else if (!isbot && (m->flags & CHANOP)) {
        to_deop_len += strlcpy(to_deop + to_deop_len, m->nick, sizeof(to_deop) - to_deop_len);
        *(to_deop + to_deop_len++) = ' ';
      }
    }
  }
//  shuffle(to_op, " ");
//  shuffle(to_deop, " ");

  size_t deops_len = 0;
  size_t work_len = 0;
  register short lines = 0;
  register const unsigned short max_lines = floodless ? 15 : 5;
  char work[2048] = "", *op = NULL, *modeline = NULL, deops[2048] = "";
  register unsigned int deopn;

  while (to_op_ptr[0] || to_deop_ptr[0]) {
    deops_len = 0;
    deopn = 0;
    op = NULL;
    modeline = NULL;
    deops[0] = 0;

    if (to_op_ptr[0])
      op = newsplit(&to_op_ptr);

    /* Prepare a list of modesperline-1 nicks for deop */
    for (register unsigned int i = 0; i < modesperline; i++) {
      if (to_deop_ptr[0] && ((i < (modesperline - 1)) || (!op))) {
        ++deopn; 
        const char *deop_nick = newsplit(&to_deop_ptr);
        deops_len += strlcpy(deops + deops_len, deop_nick, sizeof(deops) - deops_len);
        *(deops + deops_len++) = ' ';
      }
    }
    deops[deops_len] = 0;
    *(work + work_len++) = 'M';
    *(work + work_len++) = 'O';
    *(work + work_len++) = 'D';
    *(work + work_len++) = 'E';
    *(work + work_len++) = ' ';
    work_len += strlcpy(work + work_len, chan->name, sizeof(work) - work_len);
    *(work + work_len++) = ' ';

    if (deops[0])
      modeline = take_makeline(op, deops, deopn, deops_len);
    else
      modeline = take_massopline(op, &to_op_ptr);

    work_len += strlcpy(work + work_len, modeline, sizeof(work) - work_len);
    *(work + work_len++) = '\r';
    *(work + work_len++) = '\n';

    if (++lines >= max_lines) {
      tputs(serv, work, work_len);
      work[0] = 0;
      work_len = 0;
      lines = 0;
    }
  }

  if (work[0])
    tputs(serv, work, work_len);

  if (channel_closed(chan))
    enforce_closed(chan);

  enforce_bitch(chan);		/* hell why not! */

  return;
}

/* Things to do when i just became a chanop:
 */
void recheck_channel(struct chanset_t *chan, int dobans)
{
  static int stacking = 0;

  if (stacking)
    return;			/* wewps */

  if (!userlist || loading)                /* Bot doesnt know anybody */
    return;        		           /* ... it's better not to deop everybody */

  Member *m = NULL;
  char s[UHOSTLEN] = "";
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  int stop_reset = 0, botops = 0, nonbotops = 0, botnonops = 0;
  ptrlist<Member>::iterator _p;

  stacking++;

  putlog(LOG_DEBUG, "*", "recheck_channel %s", chan->dname);

  PFOR(chan->channel.hmember, Member, m) {
    bool hasop = chan_hasop(m);

    if (m) {
      if (m->user && m->user->bot && (m->user->flags & USER_OP)) {
        if (hasop)
          botops++;
        else
          botnonops++;
      } else if (hasop)
        nonbotops++;
    }
  }


  /* don't do much if i'm lonely bot. Maybe they won't notice me? :P */
  if (botops == 1 && !botnonops) {
    if (chan_bitch(chan) || channel_closed(chan))
      putlog(LOG_MISC, "*", "Opped in %s, not checking +closed/+bitch until more bots arrive.", chan->dname);
  } else {
    /* if the chan is +closed, mass deop first, safer that way. */
    if (chan_bitch(chan) || channel_closed(chan))
      enforce_bitch(chan);

    if (channel_closed(chan))
      enforce_closed(chan);
  }

  /* this can all die, we want to enforce +bitch/+take first :) */

  /* This is a bad hack for +e/+I */
  if (dobans == 2 && !channel_take(chan) && me_op(chan) && do_eI) {
    chan->channel.last_eI = now;
    if (!(chan->ircnet_status & CHAN_ASKED_EXEMPTS) && use_exempts == 1) {
        chan->ircnet_status |= CHAN_ASKED_EXEMPTS;
        dprintf(DP_MODE, "MODE %s +e\n", chan->name);
    }
    if (!(chan->ircnet_status & CHAN_ASKED_INVITES) && use_invites == 1) {
      chan->ircnet_status |= CHAN_ASKED_INVITES;
      dprintf(DP_MODE, "MODE %s +I\n", chan->name);
    }
  }

  PFOR (chan->channel.hmember, Member, m) {
    simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUHost());

    if (!m->user && !m->tried_getuser) {
           m->tried_getuser = 1;
           m->user = get_user_by_host(s);
           if (!m->user && doresolv(chan) && m->client->GetUIP()[0]) {
             simple_snprintf(s, sizeof(s), "%s!%s", m->nick, m->client->GetUIP());
             m->user = get_user_by_host(s);
           }
    }
    get_user_flagrec(m->user, &fr, chan->dname, chan);
    if (glob_bot(fr) && chan_hasop(m) && !match_my_nick(m->nick))
      stop_reset = 1;
    check_this_member(chan, m->nick, &fr);
  }

  if (dobans) {
    if (channel_nouserbans(chan) && !stop_reset)
      resetbans(chan);
    else
      recheck_bans(chan);
    if (use_invites && !(chan->ircnet_status & CHAN_ASKED_INVITES)) {
      if (channel_nouserinvites(chan) && !stop_reset)
	resetinvites(chan);
      else
	recheck_invites(chan);
    }
    if (use_exempts && !(chan->ircnet_status & CHAN_ASKED_EXEMPTS)) {
      if (channel_nouserexempts(chan) && !stop_reset)
	resetexempts(chan);
      else
	recheck_exempts(chan);
    } else {
      if (channel_enforcebans(chan)) 
        enforce_bans(chan);
    }
    flush_mode(chan, QUICK); 
    if ((chan->status & CHAN_ASKEDMODES) && !channel_inactive(chan)) 
      dprintf(DP_MODE, "MODE %s\n", chan->name);
    recheck_channel_modes(chan);
  }
  stacking--;
}

/* got 302: userhost
 * <server> 302 <to> :<nick??user@host>
 */
static int got302(char *from, char *msg)
{
  char *p = NULL, *nick = NULL, *uhost = NULL;
  Client *client = NULL;

#ifdef CACHE
  cache_t *cache = NULL;
  cache_chan_t *cchan = NULL;
#endif

  newsplit(&msg);
  fixcolon(msg);
  
  p = strchr(msg, '=');
  if (!p)
    p = strchr(msg, '*');
  if (!p)
    return 0;
  *p = 0;
  nick = msg;
  p += 2;		/* skip =|* plus the next char */
  uhost = p;

  if ((p = strchr(uhost, ' ')))
    *p = 0;

  if (!(client = Client::Find(nick))) {
    client = new Client(nick);
  }

  if (match_my_nick(nick)) {
    /* Our USERHOST comes back as an ip ... */
    client->SetUIP(uhost);
    strlcpy(botuserip, uhost, UHOSTLEN);
    simple_snprintf(meip, sizeof(meip), "%s!%s", botname, botuserip);
    sdprintf("botuserip: %s", botuserip);
    return 0;
  }

  /* .. others come back as what we see in WHOIS */
  client->SetUHost(uhost);

#ifdef CACHE
  if ((cache = cache_find(nick))) {
    if (!cache->uhost[0])
    strcpy(cache->uhost, uhost);

    if (!cache->handle[0]) {
      char s[UHOSTLEN] = "";
      struct userrec *u = NULL;

      simple_snprintf(s, sizeof(s), "%s!%s", nick, uhost);
      if ((u = get_user_by_host(s)))
        strcpy(cache->handle, u->handle);
    }
    cache->timeval = now;
 
    /* check if we should invite this client to chans */
    for (cchan = cache->cchan; cchan && cchan->dname[0]; cchan = cchan->next) {
      if (cchan->invite) {
        dprintf(DP_SERVER, "INVITE %s %s\n", nick, cchan->dname);
        cchan->invite = 0;
        cchan->invited = 1;
      }
      if (cchan->ban) {
        cchan->ban = 0;
        dprintf(DP_DUMP, "MODE %s +b *!%s\n", cchan->dname, uhost);
      }
    }
  }
#endif
  return 0;
}

#ifdef CACHE
/* got341 invited
 * <server> 341 <to> <nick> <channel>
 */
static int got341(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL;
  cache_t *cache = NULL;
  cache_chan_t *cchan = NULL;

  newsplit(&msg);
  nick = newsplit(&msg);
  chname = newsplit(&msg);

  struct chanset_t *chan = findchan(chname);

  if (!chan) {
    putlog(LOG_MISC, "*", "%s: %s", "Hmm, mode info from a channel I'm not on", chname);
    dprintf(DP_SERVER, "PART %s\n", chname);
    return 0;
  }

  cache = cache_find(nick);

  if (cache) {
    for (cchan = cache->cchan; cchan && cchan->dname; cchan = cchan->next) {
      if (!rfc_casecmp(cchan->dname, chan->dname)) {
        if (!cache->uhost[0] || !cchan->invited)
          goto hijack;

        cache->timeval = now;
        notice_invite(chan, cache->handle[0] ? cache->handle : NULL, nick, cache->uhost, cchan->op);

//        cache_del_chan_parm(cache, cache->cchan);
        break;
      }
    }
  }

  if (!cache || !cchan)
    goto hijack;

  return 0;

  hijack:

  if (!cache)
    cache = cache_new(nick);
  if (!cchan)
    cchan = cache_chan_add(cache, chan->dname);

  if (!cache->uhost[0]) {
    dprintf(DP_DUMP, "MODE %s +b %s!*@*\n", chan->name, nick);
    cchan->ban = 1;
    dprintf(DP_DUMP, "USERHOST %s\n", nick);
  } else {
    dprintf(DP_DUMP, "MODE %s +b *!*%s\n", chan->name, cache->uhost);
  }
  putlog(LOG_MISC, "*", "HIJACKED invite detected: %s to %s", nick, chan->dname);
  dprintf(DP_DUMP, "PRIVMSG %s :ALERT! \002%s was invited via a hijacked connection/process.\002\n", chan->name, nick);
  return 0;
}
#endif /* CACHE */

/* got 324: mode status
 * <server> 324 <to> <channel> <mode>
 */
static int got324(char *from, char *msg)
{
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  if (match_my_nick(chname))
      return 0;

  struct chanset_t *chan = findchan(chname);

  if (!chan) {
    putlog(LOG_MISC, "*", "%s: %s", "Hmm, mode info from a channel I'm not on", chname);
    dprintf(DP_SERVER, "PART %s\n", chname);
    return 0;
  }

  int i = 1;
  bool ok = 0;
  char *p = NULL, *q = NULL;

  if (chan->status & CHAN_ASKEDMODES)
    ok = 1;
  chan->status &= ~CHAN_ASKEDMODES;
  chan->channel.mode = 0;
  while (msg[i] != 0) {
    if (msg[i] == 'i')
      chan->channel.mode |= CHANINV;
    if (msg[i] == 'p')
      chan->channel.mode |= CHANPRIV;
    if (msg[i] == 's')
      chan->channel.mode |= CHANSEC;
    if (msg[i] == 'm')
      chan->channel.mode |= CHANMODER;
    if (msg[i] == 'c')
      chan->channel.mode |= CHANNOCLR;
    if (msg[i] == 'C')
      chan->channel.mode |= CHANNOCTCP;
    if (msg[i] == 'R')
      chan->channel.mode |= CHANREGON;
    if (msg[i] == 'M')
      chan->channel.mode |= CHANMODR;
    if (msg[i] == 'r')
      chan->channel.mode |= CHANLONLY;
    if (msg[i] == 't')
      chan->channel.mode |= CHANTOPIC;
    if (msg[i] == 'n')
      chan->channel.mode |= CHANNOMSG;
    if (msg[i] == 'a')
      chan->channel.mode |= CHANANON;
    if (msg[i] == 'q')
      chan->channel.mode |= CHANQUIET;
    if (msg[i] == 'k') {
      chan->channel.mode |= CHANKEY;
      p = strchr(msg, ' ');
      if (p != NULL) {		/* Test for null key assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  my_setkey(chan, p);
	  strcpy(p, q + 1);
	} else {
	  my_setkey(chan, p);
	  *p = 0;
	}
      }
      if ((chan->channel.mode & CHANKEY) && (!chan->channel.key[0] ||
	  !strcmp("*", chan->channel.key)))
	/* Undernet use to show a blank channel key if one was set when
	 * you first joined a channel; however, this has been replaced by
	 * an asterisk and this has been agreed upon by other major IRC 
	 * networks so we'll check for an asterisk here as well 
	 * (guppy 22Dec2001) */ 
        chan->status |= CHAN_ASKEDMODES;
    }
    if (msg[i] == 'l') {
      p = strchr(msg, ' ');
      if (p != NULL) {		/* test for null limit assignment */
	p++;
	q = strchr(p, ' ');
	if (q != NULL) {
	  *q = 0;
	  chan->channel.maxmembers = atoi(p);
/*	  strcpy(p, q + 1); */
          simple_sprintf(p, "%s", q + 1);
	} else {
	  chan->channel.maxmembers = atoi(p);
	  *p = 0;
	}
      }
    }
    i++;
  }
  if (ok)
    recheck_channel_modes(chan);
  return 0;
}

static int got352or4(struct chanset_t *chan, char *user, char *host, char *nick, char *flags, int hops, char* realname)
{
  struct flag_record fr = { FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  char userhost[UHOSTLEN] = "";
  Member *m = NULL;	/* in my channel list copy? */
  bool waschanop = 0, me = 0;

  m = ismember(chan, nick);
  
  if (!m) {		/* Nope, so update */
    m = newmember(chan, nick);	/* Get a new channel entry */
    m->joined = m->split = m->delay = 0L;	/* Don't know when he joined */
    m->flags = 0;		/* No flags for now */
    m->last = now;		/* Last time I saw him */
    m->user = NULL;
  }

  m->client->hops = hops;

  waschanop = me_op(chan);      /* Am I opped here? */
  if (strchr(flags, '@') != NULL)	/* Flags say he's opped? */
    m->flags |= (CHANOP | WASOP);	/* Yes, so flag in my table */
  else
    m->flags &= ~(CHANOP | WASOP);
  if (strchr(flags, '*'))
    m->flags |= OPER;
  else
    m->flags &= ~OPER;
  if (strchr(flags, '+') != NULL)	/* Flags say he's voiced? */
    m->flags |= CHANVOICE;	/* Yes */
  else
    m->flags &= ~CHANVOICE;
  if (!(m->flags & (CHANVOICE | CHANOP)))
    m->flags |= STOPWHO;

  /* Store the userhost */
  if (!m->client->GetUHost()[0])
    m->client->SetUHost(host, user);

  /* And realname */
  if (!m->client->GetGecos()[0])
    m->client->SetGecos(realname);

  simple_snprintf(userhost, sizeof(userhost), "%s!%s", nick, m->client->GetUHost());

  me = match_my_nick(nick);

  if (me) {			/* Is it me? */
//    strcpy(botuserhost, m->client->GetUHost());		/* Yes, save my own userhost */
    m->joined = now;				/* set this to keep the whining masses happy */

    if (!waschanop && me_op(chan))
      recheck_channel(chan, 2);
    if (!me_op(chan) && any_ops(chan))
      chan->channel.do_opreq = 1;
  }

  if (!m->user && !m->tried_getuser) {
    m->user = get_user_by_host(userhost);
    m->tried_getuser = 1;
  }

  //userhost failed, let's try resolving them...
  if (!m->user && doresolv(chan)) {
    if (is_dotted_ip(host))
      m->client->SetUIP(host, user);
    else  
      resolve_to_member(chan, nick, host);
  }

  get_user_flagrec(m->user, &fr, chan->dname, chan);
  
  if (me_op(chan)) {
    /* are they a chanop, and me too */
        /* are they a channel or global de-op */
    if (chan_hasop(m) && chk_deop(fr, chan) && !me)
        /* && target_priority(chan, m, 1) */
      add_mode(chan, '-', 'o', nick);

    /* if channel is enforce bans */
    if (channel_enforcebans(chan) &&
        !chan_sentkick(m) && 
        /* and user matches a ban */
        (u_match_mask(global_bans, userhost) || u_match_mask(chan->bans, userhost)) &&
        /* and it's not me, and i'm an op */
        !me) {
      /*  && target_priority(chan, m, 0) */
      dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, nick, bankickprefix, r_banned());
      m->flags |= SENTKICK;
    }
    /* if the user is a +k */
    else if ((chan_kick(fr) || glob_kick(fr)) &&
           !chan_sentkick(m) &&
           /* and it's not me :) who'd set me +k anyway, a sicko? */
           /* and if im an op */
           !me) {
           /* && target_priority(chan, m, 0) */
      /* cya later! */
      quickban(chan, userhost);
      dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chan->name, nick, bankickprefix, response(RES_KICKBAN));
      m->flags |= SENTKICK;
    }
  }

  return 0;
}

/* got a 352: who info!
 */
static int got352(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);		/* Skip my nick - effeciently */
  chname = newsplit(&msg);	/* Grab the channel */
  chan = findchan(chname);	/* See if I'm on channel */
  if (chan) {			/* Am I? */
    char *nick = NULL, *user = NULL, *host = NULL, *flags = NULL, *hops = NULL, *realname = NULL;

    user = newsplit(&msg);	/* Grab the user */
    host = newsplit(&msg);	/* Grab the host */
    newsplit(&msg);		/* skip the server */
    nick = newsplit(&msg);	/* Grab the nick */
    flags = newsplit(&msg);	/* Grab the flags */
    hops = newsplit(&msg);	/* grab server hops */
    ++hops;			/* Skip the : */
    realname = newsplit(&msg);	/* realname/gecos */
    got352or4(chan, user, host, nick, flags, atoi(hops), realname);
  }
  return 0;
}

/* got a 354: who info! - iru style
 */
static int got354(char *from, char *msg)
{
  if (use_354) {
    newsplit(&msg);		/* Skip my nick - effeciently */
    if (msg[0] && (strchr(CHANMETA, msg[0]) != NULL)) {
      char *chname = newsplit(&msg);	/* Grab the channel */
      struct chanset_t *chan = findchan(chname);	/* See if I'm on channel */

      if (chan) {		/* Am I? */
        char *nick = NULL, *user = NULL, *host = NULL, *flags = NULL, *hops = NULL, *realname = NULL;

	user = newsplit(&msg);	/* Grab the user */
	host = newsplit(&msg);	/* Grab the host */
	nick = newsplit(&msg);	/* Grab the nick */
	flags = newsplit(&msg);	/* Grab the flags */
        hops = newsplit(&msg);  /* yay for hops, does iru even have hops?? */
        ++hops;			/* Skip the : */
        realname = newsplit(&msg);
	got352or4(chan, user, host, nick, flags, atoi(hops), realname);
      }
    }
  }
  return 0;
}

/* got 315: end of who
 * <server> 315 <to> <chan> :End of /who
 */
static int got315(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  /* May have left the channel before the who info came in */
  if (!chan || !channel_pending(chan))
    return 0;

  /* Finished getting who list, can now be considered officially ON CHANNEL */
  chan->status |= CHAN_ACTIVE;
  chan->status &= ~(CHAN_PEND | CHAN_JOINING);
  /* Am *I* on the channel now? if not, well d0h. */
  if (shouldjoin(chan) && !ismember(chan, botname)) {
    putlog(LOG_MISC | LOG_JOIN, chan->dname, "Oops, I'm not really on %s.", chan->dname);
    clear_channel(chan, 1);
    chan->status &= ~CHAN_ACTIVE;
    chan->status |= CHAN_JOINING;
    dprintf(DP_MODE, "JOIN %s %s\n",
	    (chan->name[0]) ? chan->name : chan->dname,
	    chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  } else {
    if (me_op(chan))
      recheck_channel(chan, 2);
    else if (chan->channel.members == 1)
      chan->status |= CHAN_STOP_CYCLE;
    else
      request_op(chan);
  }
  /* do not check for i-lines here. */
  return 0;
}

/* got 367: ban info
 * <server> 367 <to> <chan> <ban> [placed-by] [timestamp]
 */
static int got367(char *from, char *origmsg)
{
  char *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf, origmsg, sizeof(buf));
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  
  char *ban = newsplit(&msg), *who = newsplit(&msg);

  /* Extended timestamp format? */
  if (who[0])
    newban(chan, ban, who);
  else
    newban(chan, ban, "existent");
  return 0;
}

/* got 368: end of ban list
 * <server> 368 <to> <chan> :etc
 */
static int got368(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  /* Okay, now add bans that i want, which aren't set yet */
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan)
    chan->status &= ~CHAN_ASKEDBANS;
  /* If i sent a mode -b on myself (deban) in got367, either
   * resetbans() or recheck_bans() will flush that.
   */
  return 0;
}

/* got 348: ban exemption info
 * <server> 348 <to> <chan> <exemption>
 */
static int got348(char *from, char *origmsg)
{
  if (use_exempts == 0)
    return 0;

  char *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf, origmsg, sizeof(buf));
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  
  char *exempt = newsplit(&msg), *who = newsplit(&msg);

  /* Extended timestamp format? */
  if (who[0])
    newexempt(chan, exempt, who);
  else
    newexempt(chan, exempt, "existent");
  return 0;
}

/* got 349: end of ban exemption list
 * <server> 349 <to> <chan> :etc
 */
static int got349(char *from, char *msg)
{
  if (use_exempts == 1) {
    struct chanset_t *chan = NULL;
    char *chname = NULL;

    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      putlog(LOG_DEBUG, "*", "END +e %s", chan->dname);
      chan->ircnet_status &= ~CHAN_ASKED_EXEMPTS;
      
      if (channel_nouserexempts(chan))
        resetexempts(chan);
      else
        recheck_exempts(chan);

      if (channel_enforcebans(chan))
        enforce_bans(chan);
    }
  }
  return 0;
}

static void got353(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg); /* my nick */
  newsplit(&msg); /*    *|@|=  */
  chname = newsplit(&msg);
  chan = findchan(chname);
  fixcolon(msg);
  irc_log(chan, "%s", msg);
}

/* got 346: invite exemption info
 * <server> 346 <to> <chan> <exemption>
 */
static int got346(char *from, char *origmsg)
{
  if (use_invites == 0)
    return 0;

  char *chname = NULL, buf[511] = "", *msg = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf, origmsg, sizeof(buf));
  msg = buf;
  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan || !(channel_pending(chan) || channel_active(chan)))
    return 0;
  
  char *invite = newsplit(&msg), *who = newsplit(&msg);

  /* Extended timestamp format? */
  if (who[0])
    newinvite(chan, invite, who);
  else
    newinvite(chan, invite, "existent");
  return 0;
}

/* got 347: end of invite exemption list
 * <server> 347 <to> <chan> :etc
 */
static int got347(char *from, char *msg)
{
  if (use_invites == 1) {
    struct chanset_t *chan = NULL;
    char *chname = NULL;

    newsplit(&msg);
    chname = newsplit(&msg);
    chan = findchan(chname);
    if (chan) {
      chan->ircnet_status &= ~CHAN_ASKED_INVITES;

      if (channel_nouserinvites(chan))
        resetinvites(chan);
      else
        recheck_invites(chan);
    }
  }
  return 0;
}

/* Too many channels.
 */
static int got405(char *from, char *msg)
{
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  putlog(LOG_MISC, "*", "I'm on too many channels--can't join: %s", chname);
  return 0;
}

/* This is only of use to us with !channels. We get this message when
 * attempting to join a non-existant !channel... The channel must be
 * created by sending 'JOIN !!<channel>'. <cybah>
 *
 * 403 - ERR_NOSUCHCHANNEL
 */
static int got403(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  if (chname && chname[0]=='!') {
    chan = findchan_by_dname(chname);
    if (!chan) {
      chan = findchan(chname);
      if (!chan)
        return 0;       /* Ignore it */
      /* We have the channel unique name, so we have attempted to join
       * a specific !channel that doesnt exist. Now attempt to join the
       * channel using it's short name.
       */
      putlog(LOG_MISC, "*",
             "Unique channel %s does not exist... Attempting to join with "
             "short name.", chname);
      dprintf(DP_SERVER, "JOIN %s\n", chan->dname);
      chan->status |= CHAN_JOINING;
    } else {
      /* We have found the channel, so the server has given us the short
       * name. Prefix another '!' to it, and attempt the join again...
       */
      putlog(LOG_MISC, "*",
             "Channel %s does not exist... Attempting to create it.", chname);
      dprintf(DP_SERVER, "JOIN !%s\n", chan->dname);
    }
  }
  return 0;
}

/* got 471: can't join channel, full
 */
static int got471(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    chan->status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Channel full--can't join: %s", chan->dname);
    request_in(chan);
/* need: limit */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, "Channel full--can't join: %s", chname);
  return 0;
}

/* got 473: can't join channel, invite only
 */
static int got473(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    chan->status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Channel invite only--can't join: %s", chan->dname);
    request_in(chan);
/* need: invite */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, "Channel invite only--can't join: %s", chname);
  return 0;
}

/* got 474: can't join channel, banned
 */
static int got474(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan) {
    chan->status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Banned from channel--can't join: %s", chan->dname);
    request_in(chan);
/* need: unban */
    chan = findchan_by_dname(chname); 
    if (!chan)
      return 0;
  } else
    putlog(LOG_JOIN, chname, "Banned from channel--can't join: %s", chname);
  return 0;
}

/* got 475: can't join channel, bad key
 */
static int got475(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  /* !channel short names (also referred to as 'description names'
   * can be received by skipping over the unique ID.
   */
  if ((chname[0] == '!') && (strlen(chname) > CHANNEL_ID_LEN)) {
    chname += CHANNEL_ID_LEN;
    chname[0] = '!';
  }
  /* We use dname because name is first set on JOIN and we might not
   * have joined the channel yet.
   */
  chan = findchan_by_dname(chname);
  if (chan && shouldjoin(chan)) {
    chan->status &= ~CHAN_JOINING;
    putlog(LOG_JOIN, chan->dname, "Bad key--can't join: %s", chan->dname);
    if (chan->channel.key[0]) {
      free(chan->channel.key);
      chan->channel.key = (char *) my_calloc(1, 1);
      dprintf(DP_MODE, "JOIN %s %s\n", chan->dname, chan->key_prot);
    } else {
      request_in(chan);
/* need: key */
    }
  } else
    putlog(LOG_JOIN, chname, "Bad key--can't join: %s", chname);
  return 0;
}

/* got invitation
 */
static int gotinvite(char *from, char *msg)
{
  char *nick = NULL;
  struct chanset_t *chan = NULL;
  bool flood = 0;

  newsplit(&msg);
  fixcolon(msg);
  nick = splitnick(&from);
  /* Two invites to the same channel in 10 seconds? */
  if (!rfc_casecmp(last_invchan, msg))
    if (now - last_invtime < 10)
      flood = 1;
  if (!flood)
    putlog(LOG_MISC, "*", "%s!%s invited me to %s", nick, from, msg);
  strncpy(last_invchan, msg, 299);
  last_invchan[299] = 0;
  last_invtime = now;
  chan = findchan(msg);
  if (!chan)
    /* Might be a short-name */
    chan = findchan_by_dname(msg);
  else {
    if (channel_pending(chan) || channel_active(chan))
      dprintf(DP_HELP, "NOTICE %s :I'm already here.\n", nick);
    else if (shouldjoin(chan))
      dprintf(DP_MODE, "JOIN %s %s\n", (chan->name[0]) ? chan->name : chan->dname,
              chan->channel.key[0] ? chan->channel.key : chan->key_prot);
  }
  return 0;
}

/* Set the topic.
 */
static void set_topic(struct chanset_t *chan, char *k)
{
  if (chan->channel.topic)
    free(chan->channel.topic);
  if (k && k[0]) {
    chan->channel.topic = (char *) my_calloc(1, strlen(k) + 1);
    strcpy(chan->channel.topic, k);
  } else
    chan->channel.topic = NULL;
}

/* Topic change.
 */
static int gottopic(char *from, char *msg)
{
  char *chname = newsplit(&msg), *nick = NULL;
  struct chanset_t *chan = NULL;
  struct userrec *u = NULL;

  fixcolon(msg);
  u = get_user_by_host(from);
  nick = splitnick(&from);
  chan = findchan(chname);
  if (chan) {
    Member *m = ismember(chan, nick);

    irc_log(chan, "%s!%s changed topic to: %s", nick, from, msg);
    if (m != NULL)
      m->last = now;
    set_topic(chan, msg);
  }
  return 0;
}

/* 331: no current topic for this channel
 * <server> 331 <to> <chname> :etc
 */
static int got331(char *from, char *msg)
{
  char *chname = NULL;
  struct chanset_t *chan = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    set_topic(chan, NULL);
  }
  return 0;
}

/* 332: topic on a channel i've just joined
 * <server> 332 <to> <chname> :topic goes here
 */
static int got332(char *from, char *msg)
{
  struct chanset_t *chan = NULL;
  char *chname = NULL;

  newsplit(&msg);
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (chan) {
    fixcolon(msg);
    set_topic(chan, msg);
  }
  return 0;
}

/* Got a join
 */
static int gotjoin(char *from, char *chname)
{
  struct chanset_t *chan = NULL;

  fixcolon(chname);
  chan = findchan(chname);
  if (!chan && chname[0] == '!') {
    /* As this is a !channel, we need to search for it by display (short)
     * name now. This will happen when we initially join the channel, as we
     * dont know the unique channel name that the server has made up. <cybah>
     */
    size_t l_chname = strlen(chname);

    if (l_chname > (CHANNEL_ID_LEN + 1)) {
      char* ch_dname = (char *) my_calloc(1, l_chname + 1);
      simple_snprintf(ch_dname, l_chname + 2, "!%s", chname + (CHANNEL_ID_LEN + 1));
      chan = findchan_by_dname(ch_dname);
      if (!chan) {
      /* Hmm.. okay. Maybe the admin's a genius and doesn't know the
       * difference between id and descriptive channel names. Search
       * the channel name in the dname list using the id-name.
       */
        chan = findchan_by_dname(chname);
        if (chan) {
          /* Duh, I was right. Mark this channel as inactive and log
           * the incident.
           */
          chan->status |= CHAN_INACTIVE;
          putlog(LOG_MISC, "*", "Deactivated channel %s, because it uses "
                                "an ID channel-name. Use the descriptive name instead.", chname);
          dprintf(DP_SERVER, "PART %s\n", chname);

          free(ch_dname);
          return 0;
        }
      }
      free(ch_dname);
    }
  } else if (!chan) {
    /* As this is not a !chan, we need to search for it by display name now.
     * Unlike !chan's, we dont need to remove the unique part.
     */
    chan = findchan_by_dname(chname);
  }

  char *nick = NULL, buf[UHOSTLEN] = "", *uhost = buf;

  strcpy(uhost, from);
  nick = splitnick(&uhost);

  if (!chan || (chan && !shouldjoin(chan))) {
    if (match_my_nick(nick)) {
      putlog(LOG_WARN, "*", "joined %s but didn't want to!", chname);
      dprintf(DP_MODE, "PART %s\n", chname);
    }
  } else if (!channel_pending(chan)) {
    char *host = NULL;

    chan->status &= ~CHAN_STOP_CYCLE;

    detect_chan_flood(nick, uhost, from, chan, FLOOD_JOIN, NULL);

    if ((host = strchr(uhost, '@')))
      ++host;

    if (!channel_active(chan) && !match_my_nick(nick)) {
      /* uh, what?!  i'm on the channel?! */
      putlog(LOG_ERROR, "*", "confused bot: guess I'm on %s and didn't realize it", chan->dname);
      chan->status |= CHAN_ACTIVE;
      chan->status &= ~(CHAN_PEND | CHAN_JOINING);
      reset_chan_info(chan);
    } else {
      Member *m = ismember(chan, nick);
      bool splitjoin = 0;

      /* Net-join */
      if (m && m->split && !egg_strcasecmp(m->client->GetUHost(), uhost)) {
        splitjoin = 1;
	m->split = 0;
	m->last = now;
	m->delay = 0L;
	m->flags = (chan_hasop(m) ? WASOP : 0);
	set_handle_laston(chan->dname, m->user, now);
	m->flags |= STOPWHO;
        irc_log(chan, "%s returned from netsplit", m->nick);


        if (m && !m->user) {
          m->user = get_user_by_host(from);
          m->tried_getuser = 1;
 
          if (!m->user && doresolv(chan)) {
            if (is_dotted_ip(host)) 
              m->client->SetUIP(uhost);
            else
              resolve_to_member(chan, nick, host); 
          }
        }
      } else {
	if (m)
	  killmember(chan, nick);
	m = newmember(chan, nick);

	m->joined = now;
	m->split = 0L;
	m->flags = 0;
	m->last = now;
	m->delay = 0L;
        m->user = get_user_by_host(from);
        m->tried_getuser = 1;

        m->client->SetUHost(uhost);

        if (!m->user && doresolv(chan)) {
          if (is_dotted_ip(host))
            m->client->SetUIP(uhost);
          else
            resolve_to_member(chan, nick, host); 
        }

	m->flags |= STOPWHO;

	if (match_my_nick(nick)) {
	  /* It was me joining! Need to update the channel record with the
	   * unique name for the channel (as the server see's it). <cybah>
	   */
	  strlcpy(chan->name, chname, 81);
	  chan->status &= ~CHAN_JUPED;

          /* ... and log us joining. Using chan->dname for the channel is
	   * important in this case. As the config file will never contain
	   * logs with the unique name.
           */
	  if (chname[0] == '!')
            irc_log(chan, "Joined. (%s)", chname);
	  else
            irc_log(chan, "Joined.");
	  if (!match_my_nick(chname))
 	    reset_chan_info(chan);
	} else {
          irc_log(chan, "Join: %s (%s)", nick, uhost);
	  set_handle_laston(chan->dname, m->user, now);
	}
      }

      /* ok, the op-on-join,etc, tests...first only both if Im opped */
      if (me_op(chan)) {
        struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

        get_user_flagrec(m->user, &fr, chan->dname, chan);

        bool is_op = chk_op(fr, chan);

        /* Check for a mass join */
        if (!splitjoin && channel_nomassjoin(chan) && !is_op) {
          if (chan->channel.drone_jointime < now - chan->flood_mjoin_time) {      //expired, reset counter
            chan->channel.drone_joins = 0;
          }
          ++chan->channel.drone_joins;
          chan->channel.drone_jointime = now;

          if (!chan->channel.drone_set_mode && chan->channel.drone_joins >= chan->flood_mjoin_thr) {  //flood from dronenet, let's attempt to set +im
            detected_drone_flood(chan, m);
          }
        }


	/* Check for and reset exempts and invites.
	 *
	 * This will require further checking to account for when to use the
	 * various modes.
	 */
	if (u_match_mask(global_invites,from) ||
	    u_match_mask(chan->invites, from))
	  refresh_invite(chan, from);

	if (!(use_exempts && (u_match_mask(global_exempts,from) || u_match_mask(chan->exempts, from)))) {
          if (channel_enforcebans(chan) && !chan_sentkick(m) && !is_op &&
              !(use_exempts && (isexempted(chan, from) || (chan->ircnet_status & CHAN_ASKED_EXEMPTS)))) {
            for (masklist* b = chan->channel.ban; b->mask[0]; b = b->next) {
              if (wild_match(b->mask, from) || match_cidr(b->mask, from)) {
                dprintf(DP_SERVER, "KICK %s %s :%s%s\n", chname, m->nick, bankickprefix, r_banned());
                m->flags |= SENTKICK;
                return 0;
              }
            }
          }
	  /* If it matches a ban, dispose of them. */
	  if (u_match_mask(global_bans, from) || u_match_mask(chan->bans, from)) {
	    refresh_ban_kick(chan, m, from);
	  /* Likewise for kick'ees */
	  } else if (!chan_sentkick(m) && (glob_kick(fr) || chan_kick(fr))) {
	    check_exemptlist(chan, from);
	    quickban(chan, from);
            dprintf(DP_MODE, "KICK %s %s :%s%s\n", chname, m->nick, bankickprefix, response(RES_KICKBAN));
	    m->flags |= SENTKICK;
	  }
	}
        bool op = 0;
#ifdef CACHE
        cache_t *cache = cache_find(nick);

        if (cache) {
          cache_chan_t *cchan = NULL;

          if (egg_strcasecmp(cache->uhost, m->client->GetUHost())) {


          }

          for (cchan = cache->cchan; cchan && cchan->dname[0]; cchan = cchan->next) {
            if (!rfc_casecmp(cchan->dname, chan->dname)) {
              if (cchan->op) {
                op = 1;
                cchan->op = 0;
              }
              break;
            }
          }
        }
#endif /* CACHE */
        if (!splitjoin && !chan_hasop(m) && (op || (dovoice(chan) && 
            !u_pass_match(m->user, "-") && chk_autoop(fr, chan)))) {
          do_op(m->nick, chan, 1, 0);
        }
      }
    }
  }

  return 0;
}

/* Got a part
 */
static int gotpart(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL;
  struct chanset_t *chan = NULL;
  char buf[UHOSTLEN] = "", *uhost = buf;

  chname = newsplit(&msg);
  fixcolon(chname);
  fixcolon(msg);
  chan = findchan(chname);

  strcpy(uhost, from);
  nick = splitnick(&uhost);

  if (chan && !shouldjoin(chan) && match_my_nick(nick)) {
    irc_log(chan, "Parting");    
    clear_channel(chan, 1);
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND | CHAN_JOINING);
    return 0;
  }
  if (chan && !channel_pending(chan)) {
    Member* m = ismember(chan, nick);
    struct userrec *u = (m && m->user) ? m->user : get_user_by_host(from);

    if (!channel_active(chan)) {
      /* whoa! */
      putlog(LOG_ERRORS, "*", "confused bot: guess I'm on %s and didn't realize it", chan->dname);
      chan->status |= CHAN_ACTIVE;
      chan->status &= ~(CHAN_PEND | CHAN_JOINING);
      reset_chan_info(chan);
    }
    set_handle_laston(chan->dname, u, now);

    if (m) {
      detect_chan_flood(nick, m->client->GetUHost(), from, chan, FLOOD_PART, NULL);
      killmember(chan, nick);
    }
    if (msg[0])
      irc_log(chan, "Part: %s (%s) [%s]", nick, uhost, msg);
    else
      irc_log(chan, "Part: %s (%s)", nick, uhost);
    /* If it was me, all hell breaks loose... */
    if (match_my_nick(nick)) {
      clear_channel(chan, 1);
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND | CHAN_JOINING);
      if (shouldjoin(chan)) {
	dprintf(DP_MODE, "JOIN %s %s\n",
	        (chan->name[0]) ? chan->name : chan->dname,
	        chan->channel.key[0] ? chan->channel.key : chan->key_prot);
        chan->status |= CHAN_JOINING;
      }
    } else
      check_lonely_channel(chan);
  }
  return 0;
}

/* Got a kick
 */
static int gotkick(char *from, char *origmsg)
{
  char buf2[511], *msg = buf2, *chname = NULL;
  struct chanset_t *chan = NULL;

  strlcpy(buf2, origmsg, sizeof(buf2));
  msg = buf2;
  chname = newsplit(&msg);
  chan = findchan(chname);
  if (!chan)
    return 0;

  char *nick = newsplit(&msg);

  if (match_my_nick(nick) && channel_pending(chan) && shouldjoin(chan) && !channel_joining(chan)) {
    chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
    chan->status |= CHAN_JOINING;
    dprintf(DP_MODE, "JOIN %s %s\n",
            (chan->name[0]) ? chan->name : chan->dname,
            chan->channel.key[0] ? chan->channel.key : chan->key_prot);
    clear_channel(chan, 1);
    return 0; /* rejoin if kicked before getting needed info <Wcc[08/08/02]> */
  }
  if (channel_active(chan)) {
    char *whodid = NULL, s1[UHOSTLEN] = "", buf[UHOSTLEN] = "", *uhost = buf;
    Member *m = NULL;
    struct userrec *u = NULL;
    struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

    chan->channel.fighting++;
    fixcolon(msg);
    u = get_user_by_host(from);
    strcpy(uhost, from);
    whodid = splitnick(&uhost);
    detect_chan_flood(whodid, uhost, from, chan, FLOOD_KICK, nick);

    chan = findchan(chname);
    if (!chan)
      return 0;     

    m = ismember(chan, whodid);
    if (m)
      m->last = now;
    /* This _needs_ to use chan->dname <cybah> */
    get_user_flagrec(u, &fr, chan->dname, chan);
    set_handle_laston(chan->dname, u, now);
 
    chan = findchan(chname);
    if (!chan)
      return 0;

    if ((m = ismember(chan, nick))) {
      struct userrec *u2 = NULL;

      simple_snprintf(s1, sizeof(s1), "%s!%s", m->nick, m->client->GetUHost());
      u2 = get_user_by_host(s1);
      set_handle_laston(chan->dname, u2, now);
//      maybe_revenge(chan, from, s1, REVENGE_KICK);
    } else {
      simple_snprintf(s1, sizeof(s1), "%s!*@could.not.loookup.hostname", nick);
    }
    irc_log(chan, "%s was kicked by %s (%s)", s1, from, msg);
    /* Kicked ME?!? the sods! */
    if (match_my_nick(nick) && shouldjoin(chan) && !channel_joining(chan)) {
      chan->status &= ~(CHAN_ACTIVE | CHAN_PEND);
      dprintf(DP_MODE, "JOIN %s %s\n",
              (chan->name[0]) ? chan->name : chan->dname,
              chan->channel.key[0] ? chan->channel.key : chan->key_prot);
      chan->status |= CHAN_JOINING;
      clear_channel(chan, 1);
    } else {
      killmember(chan, nick);
      check_lonely_channel(chan);
    }
  }
  return 0;
}

/* Got a nick change
 */
static int gotnick(char *from, char *msg)
{
  char *nick = NULL, *chname = NULL, s1[UHOSTLEN] = "", buf[UHOSTLEN] = "", *uhost = buf;
  Member *m = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };
  Client *client = NULL;

  strcpy(uhost, from);
  nick = splitnick(&uhost);
  fixcolon(msg);
  irc_log(NULL, "[%s] Nick change: %s -> %s", samechans(nick, ","), nick, msg);

  client = Client::Find(nick);

  Auth *auth = Auth::Find(uhost);
  if (auth)
    auth->NewNick(msg);

  /* We must do this because of capitalization changes */
  for (ptrlist<struct chanset_t>::iterator chan = client->chans.begin(); chan; chan++) {
    if ((m = ismember(chan, nick)))
      m->NewNick(msg);
  }

  client->NewNick(msg);

  nick = msg;

  /* Compose a nick!user@host for the new nick */
  simple_snprintf(s1, sizeof(s1), "%s!%s", nick, uhost);

  /* Users can match by nick, so a recheck is needed */
  m->user = get_user_by_host(s1);

  for (struct chanset_t *chan = chanset; chan; chan = chan->next) {
    chname = chan->dname; 

    m = ismember(chan, nick);

    if (m) {
      m->last = now;

      /*
       * Banned?
       */

      detect_chan_flood(nick, uhost, from, chan, FLOOD_NICK, NULL);

      /* don't fill the serverqueue with modes or kicks in a nickflood */
      if (chan_sentkick(m) || chan_sentdeop(m) || chan_sentop(m) ||
	  chan_sentdevoice(m) || chan_sentvoice(m))
	m->flags |= STOPCHECK;
      /* Any pending kick or mode to the old nick is lost. */
	m->flags &= ~(SENTKICK | SENTDEOP | SENTOP |
		      SENTVOICE | SENTDEVOICE);


      /* make sure they stay devoiced if EVOICE! */

      /* nick-ban or nick is +k or something? */
      if (!chan_stopcheck(m)) {
	get_user_flagrec(m->user, &fr, chan->dname, chan);
	check_this_member(chan, m->nick, &fr);
      }
    }
  }
  return 0;
}

void check_should_cycle(struct chanset_t *chan)
{
  if (!me_op(chan))
    return;

  //If there are other ops split off and i'm the only op on this side of split, cycle
  int localops = 0, localbotops = 0, splitops = 0, splitbotops = 0, localnonops = 0;
  Member *ml = NULL;
  ptrlist<Member>::iterator _p;
  
  PFOR(chan->channel.hmember, Member, ml) {
    if (chan_hasop(ml)) {
      if (chan_issplit(ml)) {
        splitops++;
        if ((ml->user) && ml->user->bot)
          splitbotops++;
      } else {
        localops++;
        if ((ml->user) && ml->user->bot)
          localbotops++;
        if (localbotops >= 2)
          return;
        if (localops > localbotops)
          return;
      }
    } else {
      if (!chan_issplit(ml))
        localnonops++;
    }
  }
  if (splitbotops > 5) {
    /* I'm only one opped here... and other side has some ops... so i'm cycling */
    if (localnonops) {
      /* need to unset any +kil first */
      dprintf(DP_MODE, "MODE %s -ilk %s\nPART %s\nJOIN %s\n", chan->name,
                            (chan->channel.key && chan->channel.key[0]) ? chan->channel.key : "",
                             chan->name, chan->name);
    } else
      dprintf(DP_MODE, "PART %s\nJOIN %s\n", chan->name, chan->name);
  }
}


/* Signoff, similar to part.
 */
static int gotquit(char *from, char *msg)
{
  char *nick = NULL, *p = NULL;
  bool split = 0;
  Member *m = NULL;
  char from2[NICKMAX + UHOSTMAX + 1] = "";
  struct userrec *u = NULL;

  strcpy(from2,from);
  u = get_user_by_host(from2);
  nick = splitnick(&from);
  fixcolon(msg);
  /* Fred1: Instead of expensive wild_match on signoff, quicker method.
   *        Determine if signoff string matches "%.% %.%", and only one
   *        space.
   */
  p = strchr(msg, ' ');
  if (p && (p == strrchr(msg, ' '))) {
    char *z1 = NULL, *z2 = NULL;

    *p = 0;
    z1 = strchr(p + 1, '.');
    z2 = strchr(msg, '.');
    if (z1 && z2 && (*(z1 + 1) != 0) && (z1 - 1 != p) &&
	(z2 + 1 != p) && (z2 != msg)) {
      /* Server split, or else it looked like it anyway (no harm in
       * assuming)
       */
      split = 1;
    } else
      *p = ' ';
  }
  if (msg[0])
    irc_log(NULL, "[%s] Quits %s (%s) (%s)", samechans(nick, ","), nick, from, msg);
  else
    irc_log(NULL, "[%s] Quits %s (%s)", samechans(nick, ","), nick, from);

  for (struct chanset_t* chan = chanset; chan; chan = chan->next) {
    if (!channel_active(chan))
      continue;

    m = ismember(chan, nick);
    if (m) {
      u = m->user ? m->user : (m->tried_getuser == 0 ? get_user_by_host(from2) : NULL);
      if (u) {
        set_handle_laston(chan->dname, u, now); /* If you remove this, the bot will crash when the user record in question
						   is removed/modified during the tcl binds below, and the users was on more
						   than one monitored channel */
      }
      if (split) {
	m->split = now;
        irc_log(chan, "%s (%s) got netsplit.", nick, from);
      } else {
	killmember(chan, nick);
	check_lonely_channel(chan);
      }
      if (channel_cycle(chan))
        check_should_cycle(chan);
    }
  }
  /* Our nick quit? if so, grab it.
   */
  if (keepnick && !match_my_nick(nick)) {
    if (!rfc_casecmp(nick, origbotname)) {
      putlog(LOG_MISC, "*", "Switching back to nick %s", origbotname);
      dprintf(DP_SERVER, "NICK %s\n", origbotname);
    }
  }
#ifdef CACHE
  /* see if they were in our cache at all */
  cache_t *cache = cache_find(nick);

  if (cache) 
    cache_del(nick, cache);
#endif /* CACHE */

  return 0;
}

/* Got a private message.
 */
static int gotmsg(char *from, char *msg)
{
  if (!strchr("&#!+@$", msg[0]))
    return 0;

  bool ignoring = match_ignore(from);
  char *to = newsplit(&msg), *realto = (to[0] == '@') ? to + 1 : to;
  struct chanset_t *chan = findchan(realto);

  if (!chan)
    return 0;			/* Private msg to an unknown channel?? */


  char buf[UHOSTLEN] = "", *nick = NULL, buf2[512] = "", *uhost = buf, *p = NULL, *p1 = NULL, *code = NULL, *ctcp = NULL;
  int ctcp_count = 0;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);

  Member *m = ismember(chan, nick);

  /* Only check if flood-ctcp is active */
  if (flood_ctcp.count && detect_avalanche(msg)) {
    u = get_user_by_host(from);
    get_user_flagrec(u, &fr, chan->dname, chan);
    /* Discard -- kick user if it was to the channel */
    if (m && me_op(chan) && 
	!chan_sentkick(m) &&
	!(use_exempts && ban_fun &&
	  /* don't kickban if permanent exempted -- Eule */
	  (u_match_mask(global_exempts, from) ||
	   u_match_mask(chan->exempts, from)))) {
      if (ban_fun) {
	check_exemptlist(chan, from);
	u_addmask('b', chan, quickban(chan, uhost), conf.bot->nick,
               "that was fun, let's do it again!", now + (60 * chan->ban_time), 0);
      }
      if (kick_fun) {
	/* This can induce kickflood - arthur2 */
	dprintf(DP_SERVER, "KICK %s %s :%sthat was fun, let's do it again!\n", chan->name, nick, kickprefix);
	m->flags |= SENTKICK;
      }
    }
    if (!ignoring) {
      putlog(LOG_MODES, "*", "Avalanche from %s!%s in %s - ignoring",
	     nick, uhost, chan->dname);
      p = strchr(uhost, '@');
      if (p)
	p++;
      else
	p = uhost;
      simple_snprintf(buf2, sizeof(buf2), "*!*@%s", p);
      addignore(buf2, conf.bot->nick, "ctcp avalanche", now + (60 * ignore_time));
    }
    return 0;
  }
  /* Check for CTCP: */
  ctcp_reply[0] = 0;
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strcpy(ctcp, p1);
      strcpy(p1 - 1, p + 1);
      detect_chan_flood(nick, uhost, from, chan,
			strncmp(ctcp, "ACTION ", 7) ?
			FLOOD_CTCP : FLOOD_PRIVMSG, NULL);

      chan = findchan(realto);
      if (!chan)
        return 0;

      /* Respond to the first answer_ctcp */
      p = strchr(msg, 1);
      if (ctcp_count < answer_ctcp) {
	ctcp_count++;
	if (ctcp[0] != ' ') {
	  code = newsplit(&ctcp);
	  u = get_user_by_host(from);
	  if (!ignoring || trigger_on_ignore) {
	    if (check_bind_ctcp(nick, uhost, u, to, code, ctcp) == BIND_RET_LOG) {

	      chan = findchan(realto); 
	      if (!chan)
		return 0;

	      update_idle(chan->dname, nick);
            }
	      /* Log DCC, it's to a channel damnit! */
	      if (!strcmp(code, "ACTION")) {
                irc_log(chan, "* %s %s", nick, ctcp);
	      } else {
                irc_log(chan, "CTCP %s: from %s (%s) to %s: %s", code, nick, from, to, ctcp);
	      }
	  }
	}
      }
    }
  }
  /* Send out possible ctcp responses */
  if (ctcp_reply[0]) {
    if (ctcp_mode != 2) {
      dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
    } else {
      if (now - last_ctcp > flood_ctcp.time) {
	dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	count_ctcp = 1;
      } else if (count_ctcp < flood_ctcp.count) {
	dprintf(DP_HELP, "NOTICE %s :%s\n", nick, ctcp_reply);
	count_ctcp++;
      }
      last_ctcp = now;
    }
  }
  if (msg[0]) {
    int botmatch = 0;
    char *my_msg = NULL, *my_ptr = NULL, *fword = NULL;

    if (me_op(chan) && doflood(chan))
      detect_offense(m, chan, msg);

    /* Check even if we're ignoring the host. (modified by Eule 17.7.99) */
    detect_chan_flood(nick, uhost, from, chan, FLOOD_PRIVMSG, NULL);
    
    if (auth_chan) {
      my_msg = my_ptr = strdup(msg);
      fword = newsplit(&my_msg);		/* take out first word */
      /* the first word is a wildcard match to our nick. */
      botmatch = wild_match(fword, botname);
      if (botmatch && strcmp(fword, "*"))	
        fword = newsplit(&my_msg);	/* now this will be the command */
      /* is it a cmd? */
      if (auth_prefix[0] && fword && fword[0] && fword[1] && ((botmatch && fword[0] != auth_prefix[0]) || (fword[0] == auth_prefix[0]))) {
        Auth *auth = Auth::Find(uhost);

        if (auth && auth->Authed()) {
          if (fword[0] == auth_prefix[0])
            fword++;
          auth->atime = now;
          check_bind_authc(fword, auth, chan->dname, my_msg);
        }
      }
      free(my_ptr);
    }
    irc_log(chan, "%s<%s> %s", to[0] == '@' ? "@" : "", nick, msg);
    update_idle(chan->dname, nick);
  }
  return 0;
}

/* Got a private notice.
 */
static int gotnotice(char *from, char *msg)
{
  if (!strchr(CHANMETA "@", *msg))
    return 0;
  
  bool ignoring = match_ignore(from);
  char *to = newsplit(&msg), *realto = (*to == '@') ? to + 1 : to;
  struct chanset_t *chan = findchan(realto); 

  chan = findchan(realto);
  if (!chan)
    return 0;			/* Notice to an unknown channel?? */

  char *nick = NULL, buf2[512] = "", *p = NULL, *p1 = NULL, buf[512] = "", *uhost = buf;
  char *ctcp = NULL, *code = NULL;
  struct userrec *u = NULL;
  struct flag_record fr = {FR_GLOBAL | FR_CHAN, 0, 0, 0 };

  fixcolon(msg);
  strcpy(uhost, from);
  nick = splitnick(&uhost);
  u = get_user_by_host(from);
  if (flood_ctcp.count && detect_avalanche(msg)) {
    Member *m = ismember(chan, nick);

    get_user_flagrec(u, &fr, chan->dname, chan);
    /* Discard -- kick user if it was to the channel */
    if (me_op(chan) && m && !chan_sentkick(m) &&
	!(use_exempts && ban_fun &&
	  /* don't kickban if permanent exempted -- Eule */
	  (u_match_mask(global_exempts,from) ||
	   u_match_mask(chan->exempts, from)))) {
      if (ban_fun) {
	check_exemptlist(chan, from);
	u_addmask('b', chan, quickban(chan, uhost), conf.bot->nick,
               "that was fun, let's do it again!", now + (60 * chan->ban_time), 0);
      }
      if (kick_fun) {
	/* This can induce kickflood - arthur2 */
	dprintf(DP_SERVER, "KICK %s %s :%sthat was fun, let's do it again!\n", chan->name, nick, kickprefix);
	m->flags |= SENTKICK;
      }
    }
    if (!ignoring)
      putlog(LOG_MODES, "*", "Avalanche from %s", from);
    return 0;
  }
  /* Check for CTCP: */
  p = strchr(msg, 1);
  while (p && *p) {
    p++;
    p1 = p;
    while ((*p != 1) && *p)
      p++;
    if (*p == 1) {
      *p = 0;
      ctcp = buf2;
      strcpy(ctcp, p1);
      strcpy(p1 - 1, p + 1);
      p = strchr(msg, 1);
      detect_chan_flood(nick, uhost, from, chan,
			strncmp(ctcp, "ACTION ", 7) ?
			FLOOD_CTCP : FLOOD_PRIVMSG, NULL);

      chan = findchan(realto); 
      if (!chan)
	return 0;

      if (ctcp[0] != ' ') {
	code = newsplit(&ctcp);
	if (!ignoring || trigger_on_ignore) {
	  check_bind_ctcr(nick, uhost, u, chan->dname, code, msg);

	  chan = findchan(realto); 
	  if (!chan)
	    return 0;
          irc_log(chan, "CTCP reply %s: %s from %s (%s) to %s", code, msg, nick, from, chan->dname);
          update_idle(chan->dname, nick);
	}
      }
    }
  }
  if (msg[0]) {
    /* Check even if we're ignoring the host. (modified by Eule 17.7.99) */
    detect_chan_flood(nick, uhost, from, chan, FLOOD_NOTICE, NULL);

    chan = findchan(realto); 
    if (!chan)
      return 0;

    update_idle(chan->dname, nick);

    if (!ignoring)
      irc_log(chan, "-%s:%s- %s", nick, to, msg);
  }
  return 0;
}

static cmd_t irc_raw[] =
{
  {"302",       "",     (Function) got302,      "irc:302", LEAF},
#ifdef CACHE
  {"341",       "",     (Function) got341,      "irc:341", LEAF},
#endif /* CACHE */
  {"324",	"",	(Function) got324,	"irc:324", LEAF},
  {"352",	"",	(Function) got352,	"irc:352", LEAF},
  {"354",	"",	(Function) got354,	"irc:354", LEAF},
  {"315",	"",	(Function) got315,	"irc:315", LEAF},
  {"367",	"",	(Function) got367,	"irc:367", LEAF},
  {"368",	"",	(Function) got368,	"irc:368", LEAF},
  {"403",	"",	(Function) got403,	"irc:403", LEAF},
  {"405",	"",	(Function) got405,	"irc:405", LEAF},
  {"471",	"",	(Function) got471,	"irc:471", LEAF},
  {"473",	"",	(Function) got473,	"irc:473", LEAF},
  {"474",	"",	(Function) got474,	"irc:474", LEAF},
  {"475",	"",	(Function) got475,	"irc:475", LEAF},
  {"INVITE",	"",	(Function) gotinvite,	"irc:invite", LEAF},
  {"TOPIC",	"",	(Function) gottopic,	"irc:topic", LEAF},
  {"331",	"",	(Function) got331,	"irc:331", LEAF},
  {"332",	"",	(Function) got332,	"irc:332", LEAF},
  {"JOIN",	"",	(Function) gotjoin,	"irc:join", LEAF},
  {"PART",	"",	(Function) gotpart,	"irc:part", LEAF},
  {"KICK",	"",	(Function) gotkick,	"irc:kick", LEAF},
  {"NICK",	"",	(Function) gotnick,	"irc:nick", LEAF},
  {"QUIT",	"",	(Function) gotquit,	"irc:quit", LEAF},
  {"PRIVMSG",	"",	(Function) gotmsg,	"irc:msg", LEAF},
  {"NOTICE",	"",	(Function) gotnotice,	"irc:notice", LEAF},
  {"MODE",	"",	(Function) gotmode,	"irc:mode", LEAF},
  {"346",	"",	(Function) got346,	"irc:346", LEAF},
  {"347",	"",	(Function) got347,	"irc:347", LEAF},
  {"348",	"",	(Function) got348,	"irc:348", LEAF},
  {"349",	"",	(Function) got349,	"irc:349", LEAF},
  {"353",	"",	(Function) got353,	"irc:353", LEAF},
  {NULL,	NULL,	NULL,			NULL, 0}
};
