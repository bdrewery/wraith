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
#include <memory>

#include "common.h"
#include "binds_script.h"
#include "script.h"
#include "core_binds.h"
#include "binds.h"

#include <bdlib/src/Array.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/String.h>

void script_bind_callback(struct script_callback* callback_data, ...) {
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

  /* Set the lastbind variable before evaluating the proc so that the name
   * of the command that triggered the bind will be available to the proc.
   * This feature is used by scripts such as userinfo.tcl
   */
  script_eval("tcl", bd::String::printf("set lastbind %s", callback_data->mask.c_str()));

  // Forward to the Script callback
  ContextNote("bind callback", callback_data->callback_command->cmd.c_str());
  script_eval("tcl", "set errorInfo {}");

  callback_data->callback_command->call(args);

  /* XXX: This sucks, should detect error from above. */
  bd::String result(script_eval("tcl", "set errorInfo"));
  if (result) {
    putlog(LOG_MISC, "*", "Tcl error [%s]: %s",
        callback_data->callback_command->cmd.c_str(),
        result.c_str());
  }
}

static bd::HashTable<struct script_callback*,
  std::shared_ptr<struct script_callback>> _bind_callback_datas;

bd::String script_bind(const bd::String type, const bd::String flags,
    const bd::String mask, bd::ScriptCallbackerPtr callback_command)
{
  bind_table_t* table = bind_table_lookup(type.c_str());

  if (!table) {
    bd::String error;
    bd::Array<bd::String> tables;

    error = bd::String("invalid type: ") + type + ", should be one of: ";
    tables = bind_tables();
    error += tables.join(", ");
    throw error;
  }

  bd::String name(bd::String::printf("*%s:%s", table->name, mask.c_str()));

  if (!callback_command) {
    bd::Array<bd::String> entries;

    entries = bind_entries(table, mask);
    return entries.join(" ");
  }

  if (!(table->flags & BIND_STACKABLE)) {
    /*
     * This is a layer violation but there's not really a better way without
     * major overhaul.
     */
    bind_entry_t *old_entry = bind_entry_lookup(table, -1, mask.c_str(),
        name.c_str(), (Function) script_bind_callback);
    /* There's an old entry that needs to have its callback_data freed first. */
    if (old_entry->client_data)
      _bind_callback_datas.remove(
          static_cast<struct script_callback*>(old_entry->client_data));
  }

  auto callback_data = std::make_shared<struct script_callback>(
      callback_command, mask, table);
  bind_entry_add(table, flags.c_str(), BIND_WANTS_CD, mask.c_str(),
      name.c_str(), 0, (Function) script_bind_callback,
      callback_data.get());
  _bind_callback_datas[callback_data.get()] = callback_data;

  return mask;
}

bd::String script_unbind(const bd::String type, const bd::String flags,
    const bd::String mask, bd::ScriptCallbackerPtr callback_command)
{
  bind_table_t* table = bind_table_lookup(type.c_str());
  bd::String name(bd::String::printf("*%s:%s", table->name, mask.c_str()));
  bind_entry_t *entry = bind_entry_lookup(table, -1, mask.c_str(),
        name.c_str(), (Function) script_bind_callback);

  if (entry && entry->client_data)
      _bind_callback_datas.remove(
          static_cast<struct script_callback*>(entry->client_data));
  if (!entry) {
    /* (eggdrop) Don't error if trying to re-unbind a builtin */
    if (callback_command->cmd[0] != '*' || callback_command->cmd[4] != ':' ||
        strcmp(mask.c_str(), &(*callback_command->cmd)[5]) ||
        strncmp(type.c_str(), &(*callback_command->cmd)[1], 3)) {
      throw bd::String("no such binding");
    }
    /* Eggdrop returns mask here but whatever. */
    return "";
  }

  bind_entry_del(table, entry ? entry->id : -1, mask.c_str(), name.c_str(),
      (Function) script_bind_callback);

  return mask;
}

void binds_script_init() {
  script_add_command("bind",	script_bind,	"type flags cmd/mask ?procname?",		3);
  script_add_command("unbind",	script_unbind,	"type flags cmd/mask procname");
}

/* vim: set sts=2 sw=2 ts=8 et: */
