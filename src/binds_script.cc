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
 * core_binds.c -- handles:
 *
 *   binds for the CORE
 *
 */


#include "common.h"
#include "binds_script.h"
#include "script.h"
#include "core_binds.h"
#include "binds.h"

#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

struct script_callback {
  public:
    const bd::String interp;
    const bd::String callback_command;
    script_callback() = delete;
    script_callback(const bd::String _interp, bd::String _callback_command) : interp(_interp), callback_command(_callback_command) {};
};

void my_callback(script_callback* callback_data, const char* nick, const char* uhost, struct userrec* u, const char* args) {
  bd::String x(callback_data->callback_command);
  putlog(LOG_MISC, "*", "x: %s", x.c_str());
  // Forward to the TCL callback
  script_eval(callback_data->interp, callback_data->callback_command + bd::String::printf(" %s %s %s %s", nick, uhost, u->handle, args));
}

bd::String script_bind(bd::String type, bd::String flags, bd::String mask, bd::String cmd) {
  bind_table_t* table = bind_table_lookup(type.c_str());

  if (!table) {
    return "invalid type: " + type;
  }

  const bd::String interp("tcl");

  if (cmd) {
    bd::String name(bd::String::printf("*%s:%s", table->name, mask.c_str()));
    script_callback* callback_data = new script_callback(interp, cmd);
    bind_entry_add(table, flags.c_str(), BIND_WANTS_CD, mask.c_str(), name.c_str(), 0, (Function) my_callback, (void*) callback_data);
  }

  return bd::String();
}

void binds_script_init() {
  script_add_command("bind", script_bind);
}

/* vim: set sts=2 sw=2 ts=8 et: */
