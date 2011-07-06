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
 * script.c -- handles:
 *  TCL
 *
 */


#include "common.h"
#include "main.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/ScriptInterpTCL.h>

#include "script.h"
#include "libtcl.h"

bd::HashTable< bd::String, bd::ScriptInterp* > ScriptInterps;

void initialize_binds_tcl();

int init_script() {

#ifdef USE_SCRIPT_TCL
  if (!ScriptInterps.contains("tcl")) {
    load_libtcl();

    // create interp
    ScriptInterps["tcl"] = new bd::ScriptInterpTCL;
    initialize_binds_tcl();
  }
#endif
  return 0;
}

#ifdef USE_SCRIPT_TCL

#include "chanprog.h"

bd::String cmd_privmsg(bd::ScriptInterp& interp, const bd::ScriptArgs& args, bd::ScriptInterp::script_clientdata_t clientData) {
  if (args.length() != 3) {
    return "wrong # args: should be: channel string";
  }
  bd::String my_cd = (clientData ? *(bd::String*) clientData : bd::String());
  bd::String channel(args.getArgString(1)), msg(args.getArgString(2));
  privmsg(channel, msg, DP_SERVER);
  return "";
}


void initialize_binds_tcl() {
  ScriptInterps["tcl"]->createCommand("privmsg", cmd_privmsg);
}

#endif

int unload_script() {
  bd::Array< bd::String > keys(ScriptInterps.keys());
  for (size_t i = 0; i < keys.length(); ++i) {
    delete ScriptInterps[keys[i]];
  }
  ScriptInterps.clear();
  return 1;
}

#ifdef USE_SCRIPT_TCL
bd::String tcl_eval(const bd::String& str) {
  init_script();
  if (!ScriptInterps.contains("tcl")) return bd::String();
  return ScriptInterps["tcl"]->eval(str);
}
#endif

/* vim: set sts=2 sw=2 ts=8 et: */
