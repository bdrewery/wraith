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

/*
 * script.c -- part of irc.mod
 *   Handle scripting needs
 *
 */

#include "src/script.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

static int _script_isflag(const bd::String& nick, const bd::String& channel,
    int type) {
  memberlist *m;
  struct chanset_t *chan, *thechan = NULL;

  if (channel) {
    chan = findchan_by_dname(channel.c_str());
    thechan = chan;
    if (!chan) {
      throw bd::String("invalid channel ") + channel;
    }
  } else
    chan = chanset;

  while (chan && (thechan == NULL || thechan == chan)) {
    m = ismember(chan, nick.c_str());
    if (m) {
      if ((type == CHAN_FLAG_OP && chan_hasop(m)) ||
          (type == CHAN_FLAG_VOICE && chan_hasvoice(m))) {
        return 1;
      }
    }
    chan = chan->next;
  }
  return 0;
}

int script_botisop(bd::String channel) {
  return _script_isflag(botname, channel, CHAN_FLAG_OP);
}

int script_botisvoice(bd::String channel) {
  return _script_isflag(botname, channel, CHAN_FLAG_VOICE);
}

int script_isop(bd::String nick, bd::String channel) {
  return _script_isflag(nick, channel, CHAN_FLAG_OP);
}

int script_isvoice(bd::String nick, bd::String channel) {
  return _script_isflag(nick, channel, CHAN_FLAG_VOICE);
}

static int _script_onchan(const bd::String& nick, const bd::String& channel) {
  struct chanset_t *chan, *thechan = NULL;

  if (channel) {
    chan = findchan_by_dname(channel.c_str());
    thechan = chan;
    if (!chan) {
      throw bd::String("invalid channel ") + channel;
    }
  } else
    chan = chanset;

  while (chan && (thechan == NULL || thechan == chan)) {
    if (ismember(chan, nick.c_str())) {
      return 1;
    }
    chan = chan->next;
  }
  return 0;
}

int script_bot_onchan(bd::String channel) {
  return _script_onchan(botname, channel);
}

int script_onchan(bd::String nick, bd::String channel) {
  return _script_onchan(nick, channel);
}

long script_getchanidle(bd::String nick, bd::String channel) {
  memberlist *m;
  struct chanset_t *chan;

  chan = findchan_by_dname(channel.c_str());
  if (!chan) {
    throw bd::String("invalid channel ") + channel;
  }
  m = ismember(chan, nick.c_str());

  if (m) {
    return (now - (m->last)) / 60;
  }
  return -1;
}

bd::String script_getchanhost(bd::String nick, bd::String channel) {
  struct chanset_t *chan, *thechan = NULL;
  memberlist *m;

  if (channel) {
    chan = findchan_by_dname(channel.c_str());
    thechan = chan;
    if (!chan) {
      throw bd::String("invalid channel ") + channel;
    }
  } else
    chan = chanset;

  while (chan && (thechan == NULL || thechan == chan)) {
    m = ismember(chan, nick.c_str());
    if (m) {
      return m->userhost;
    }
    chan = chan->next;
  }
  return "";
}

void script_privmsg(bd::String channel, bd::String msg) {
  if (strchr(CHANMETA, channel[0]) && !findchan_by_dname(channel.c_str())) {
    throw bd::String("invalid channel ") + channel;
  }
  privmsg(channel, msg, DP_SERVER);
}

bd::String script_chanlist(bd::String channel, bd::String flags) {
  struct chanset_t *chan = findchan_by_dname(channel.c_str());
  if (!chan) {
    throw bd::String("invalid channel ") + channel;
  }
  bd::Array<bd::String> results;

  // No flags, return all
  if (!flags) {
    for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
      results << m->nick;
    }
  } else {
    struct flag_record plus = { FR_CHAN | FR_GLOBAL, 0, 0, 0 },
                       minus = { FR_CHAN | FR_GLOBAL, 0, 0, 0 },
                       fluser = { FR_CHAN | FR_GLOBAL, 0, 0, 0 };

    break_down_flags(flags.c_str(), &plus, &minus);
    bool have_minus = (minus.global || minus.chan || minus.bot);
    // Return empty set if asked for flags but flags don't exist
    if (!plus.global && !plus.chan && !plus.bot && !have_minus) {
      return bd::String();
    }

    plus.match |= FR_AND;
    minus.match = plus.match ^ (FR_AND | FR_OR);
    for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
      member_getuser(m);

      get_user_flagrec(m->user, &fluser, chan->dname);
      fluser.match = plus.match;
      if (flagrec_eq(&plus, &fluser) && (!have_minus || !flagrec_eq(&minus, &fluser))) {
          results << m->nick;
      }
    }
  }

  return results.join(" ");
}

static void irc_script_init() {
  script_add_command("bot_onchan", script_bot_onchan, "?channel?", 0);
  script_add_command("botisop", script_botisop, "?channel?", 0);
  script_add_command("botisvoice", script_botisvoice, "?channel?", 0);
  script_add_command("chanlist", script_chanlist, "channel ?flags?", 1);
  script_add_command("getchanhost", script_getchanhost, "nickname ?channel?", 1);
  script_add_command("getchanidle", script_getchanidle, "nickname channel");
  script_add_command("isop", script_isop, "nickname ?channel?", 1);
  script_add_command("isvoice", script_isvoice, "nickname ?channel?", 1);
  script_add_command("onchan", script_onchan, "nickname ?channel?", 1);
  script_add_command("privmsg", script_privmsg, "channel msg");
#if defined(USE_SCRIPT_TCL) && defined(DEBUG)
  script_eval("tcl", "source script.tcl");
#endif
}

/* vim: set sts=2 sw=2 ts=8 et: */
