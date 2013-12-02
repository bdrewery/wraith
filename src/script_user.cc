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
 * script_user.c -- misc scripting commands
 *
 */

#include "common.h"
#include "flags.h"
#include "script.h"
#include "userrec.h"
#include <bdlib/src/String.h>

int script_matchattr(const bd::String handle, const bd::String flags, const bd::String channel) {
  struct userrec *u;
  struct flag_record plus, minus, user;
  int ok = 0, f;

  if ((u = get_user_by_handle(userlist, handle.c_str()))) {
    user.match = FR_GLOBAL | (channel ? FR_CHAN : 0) | FR_BOT;
    get_user_flagrec(u, &user, channel ? channel.c_str() : NULL);
    plus.match = user.match;
    break_down_flags(flags.c_str(), &plus, &minus);
    /*f = (minus.global || minus.udef_global || minus.chan || minus.udef_chan ||
         minus.bot);*/
    f = (minus.global || minus.chan || minus.bot);
    if (flagrec_eq(&plus, &user)) {
      if (!f)
        ok = 1;
      else {
        minus.match = plus.match ^ (FR_AND | FR_OR);
        if (!flagrec_eq(&minus, &user))
          ok = 1;
      }
    }
  }

  return ok;
}


void init_script_user() {
  script_add_command("matchattr",	script_matchattr,	"handle flags ?channel?",			2);
  script_add_command("matchchanattr",	script_matchattr,	"handle flags ?channel?",			2);
}

/* vim: set sts=2 sw=2 ts=8 et: */
