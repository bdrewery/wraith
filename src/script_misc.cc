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
 * script_misc.c -- misc scripting commands
 *
 */

#include "script.h"
#include <bdlib/src/String.h>

void script_putlog(const bd::String text) {
  putlog(LOG_MISC, "*", "%s", text.c_str());
}

void script_putcmdlog(const bd::String text) {
  putlog(LOG_CMDS, "*", "%s", text.c_str());
}

void init_script_misc() {
  script_add_command("putlog", script_putlog, "text");
  script_add_command("putcmdlog", script_putcmdlog, "text");
}

/* vim: set sts=2 sw=2 ts=8 et: */
