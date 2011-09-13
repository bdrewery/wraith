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

SCRIPT_FUNCTION(cmd_privmsg) {
  SCRIPT_BADARGS(3, 3, " channel string");
  bd::String channel(args.getArgString(1)), msg(args.getArgString(2));
  if (strchr(CHANMETA, channel[0]) && !findchan_by_dname(channel.c_str())) {
    SCRIPT_RETURN_STRING("invalid channel: " + channel);
    return bd::SCRIPT_ERROR;
  }
  privmsg(channel, msg, DP_SERVER);
  return bd::SCRIPT_OK;
}

SCRIPT_FUNCTION(cmd_chanlist) {
  SCRIPT_BADARGS(2, 3, " channel ?flags?");

  bd::String channel(args.getArgString(1));
  chanset_t *chan = findchan_by_dname(channel.c_str());
  if (!chan) {
    SCRIPT_RETURN_STRING("invalid channel: " + channel);
    return bd::SCRIPT_ERROR;
  }
  bd::Array<bd::String> results;

  // No flags, return all
  if (args.length() == 2) {
    for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
      results << m->nick;
    }
  } else {
    struct flag_record plus = { FR_CHAN | FR_GLOBAL, 0, 0, 0 },
                       minus = { FR_CHAN | FR_GLOBAL, 0, 0, 0 },
                       fluser = { FR_CHAN | FR_GLOBAL, 0, 0, 0 };
    bd::String flags(args.getArgString(2));

    break_down_flags(flags.c_str(), &plus, &minus);
    int f = (minus.global || minus.chan || minus.bot);
    // Return empty set if asked for flags but flags don't exist
    if (!plus.global && !plus.chan && !plus.bot && !f) {
      return bd::SCRIPT_OK;
    }

    minus.match = plus.match ^ (FR_AND | FR_OR);
    for (memberlist *m = chan->channel.member; m && m->nick[0]; m = m->next) {
      member_getuser(m);

      get_user_flagrec(m->user, &fluser, chan->dname);
      fluser.match = plus.match;
      if (flagrec_eq(&plus, &fluser) && (!f || !flagrec_eq(&minus, &fluser))) {
          results << m->nick;
      }
    }
  }

  SCRIPT_RETURN_STRING(results.join(" "));
  return bd::SCRIPT_OK;
}

script_cmd_t irc_cmds[] = {
  {"privmsg",                         cmd_privmsg},
  {"chanlist",                        cmd_chanlist},
  {NULL,                              NULL}
};

/* vim: set sts=2 sw=2 ts=8 et: */
