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

#include "script.h"
#include "libtcl.h"

#ifdef USE_SCRIPT_TCL
Tcl_Interp *global_interp = NULL;
#endif

void initialize_binds_tcl();

int init_script() {

#ifdef USE_SCRIPT_TCL
  if (!global_interp) {
    load_libtcl();

    // create interp
    global_interp = Tcl_CreateInterp();
    Tcl_FindExecutable(binname);

    if (Tcl_Init(global_interp) != TCL_OK) {
      sdprintf("Tcl_Init error: %s", Tcl_GetStringResult(global_interp));
      return 1;
    }

    initialize_binds_tcl();
  }
#endif
  return 0;
}

#ifdef USE_SCRIPT_TCL

#include "chanprog.h"
static int cmd_privmsg STDVAR {
  BADARGS(3, 999, " channel string");
  bd::String str = argv[2];
  for (int i = 3; i < argc; ++i)
    str += " " + bd::String(argv[i]);
  privmsg(argv[1], str, DP_SERVER);

  return TCL_OK;
}

void initialize_binds_tcl() {
  Tcl_CreateCommand(global_interp, "privmsg", (Tcl_CmdProc*) cmd_privmsg, NULL, NULL);
}

#endif

int unload_script() {
#ifdef USE_SCRIPT_TCL
  if (global_interp) {
    Tcl_DeleteInterp(global_interp);
    global_interp = NULL;
  }
#endif
  return 1;
}

#ifdef USE_SCRIPT_TCL
bd::String tcl_eval(const bd::String& str) {
  load_libtcl();
  if (!global_interp) return bd::String();
  if (Tcl_Eval(global_interp, str.c_str()) == TCL_OK) {
    return Tcl_GetStringResult(global_interp);
  } else
    return tcl_eval("set errorInfo");
  return bd::String();
}
#endif

/* vim: set sts=2 sw=2 ts=8 et: */
