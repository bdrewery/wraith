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
    return_string = "invalid channel: " + channel;
    return SCRIPT_ERROR;
  }
  privmsg(channel, msg, DP_SERVER);
  return SCRIPT_OK;
}

void irc_init_script(bd::ScriptInterp& interp) {
    interp.createCommand("privmsg", cmd_privmsg);
}

/* vim: set sts=2 sw=2 ts=8 et: */
