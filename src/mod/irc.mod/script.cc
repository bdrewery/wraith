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

void script_privmsg(bd::String channel, bd::String msg) {
  if (strchr(CHANMETA, channel[0]) && !findchan_by_dname(channel.c_str())) {
    throw bd::String("invalid channel ") + channel;
  }
  privmsg(channel, msg, DP_SERVER);
}

bd::String script_chanlist(bd::String channel, bd::String flags) {
  chanset_t *chan = findchan_by_dname(channel.c_str());
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
  script_add_command("privmsg", script_privmsg, "channel msg");
  script_add_command("chanlist", script_chanlist, "channel ?flags?", 1);
}

/* vim: set sts=2 sw=2 ts=8 et: */
