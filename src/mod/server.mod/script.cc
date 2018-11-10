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
 * script.c -- part of server.mod
 *   Handle scripting needs
 *
 */

#include "src/script.h"
#include <bdlib/src/String.h>

static void script_put(int queue, int queue_next, const bd::String& text, const bd::String& options) {
  if (options && options != "-next" && options != "-normal")
    throw bd::String("Unknown option '") + options +
      bd::String("'. Should be one of: -normal, -next");

  if (options == "-next")
    queue = queue_next;
  dprintf(queue, "%s\n", text.c_str());
}

void script_putserv(bd::String text, bd::String options) {
  script_put(DP_SERVER, DP_SERVER_NEXT, text, options);
}
void script_putquick(bd::String text, bd::String options) {
  script_put(DP_MODE, DP_MODE_NEXT, text, options);
}
void script_puthelp(bd::String text, bd::String options) {
  script_put(DP_HELP, DP_HELP_NEXT, text, options);
}
void script_putnow(bd::String text, bd::String options) {
}

static void server_script_init() {
  script_add_command("putserv", script_putserv, "text ?options?", 1);
  script_add_command("putquick", script_putquick, "text ?options?", 1);
  script_add_command("puthelp", script_puthelp, "text ?options?", 1);
  script_add_command("putnow", script_putnow, "text ?options?", 1);
}

/* vim: set sts=2 sw=2 ts=8 et: */
