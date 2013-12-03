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

#include <stdarg.h>

#include "common.h"
#include "binds_script.h"
#include "script.h"
#include "core_binds.h"
#include "binds.h"

#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

void script_bind_callback(script_callback* callback_data, ...) {
  va_list va;
  char *type;
  bind_table_t* table = (bind_table_t*)callback_data->cdata;
  bd::Array<bd::String> args(strlen(table->syntax));
  struct userrec *u = NULL;

  // Go over the syntax and parse out the passed in args and then convert to Strings
  // to pass into the script interp
  va_start(va, callback_data);
  for (type = table->syntax; *type != '\0'; ++type) {
    switch (*type) {
      case 's':
        args << bd::String(va_arg(va, char*));
        break;
      case 'U':
        u = va_arg(va, struct userrec*);
        args << bd::String(u ? u->handle : "*");
        break;
      case 'i':
        args << bd::String::printf("%d", va_arg(va, int));
        break;
    }
  }
  va_end(va);

  // Forward to the Script callback
  ContextNote("bind callback", callback_data->callback_command->cmd.c_str());
  callback_data->callback_command->call(args);
}

bd::String script_bind(const bd::String type, const bd::String flags, const bd::String mask, bd::ScriptCallbacker* callback_command) {
  bind_table_t* table = bind_table_lookup(type.c_str());

  if (!table) {
    bd::String error;
    bd::Array<bd::String> tables;

    error = bd::String("invalid type: ") + type + ", should be one of: ";
    tables = bind_tables();
    error += tables.join(", ");
    throw error;
  }

  if (callback_command) {
    bd::String name(bd::String::printf("*%s:%s", table->name, mask.c_str()));
    script_callback* callback_data = new script_callback(callback_command, table);
    bind_entry_add(table, flags.c_str(), BIND_WANTS_CD, mask.c_str(), name.c_str(), 0, (Function) script_bind_callback, (void*) callback_data);
    return mask;
  }

  return bd::String();
}

void binds_script_init() {
  script_add_command("bind", script_bind, "type flags cmd/mask ?procname?");
}

/* vim: set sts=2 sw=2 ts=8 et: */
