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
 *  Generic Script handling
 *
 */


#include "common.h"
#include "main.h"
#include "libtcl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/ScriptInterpTCL.h>

#include "script.h"
#include "script_misc.h"
#include "script_user.h"

bd::HashTable< bd::String, bd::ScriptInterp* > ScriptInterps;

int init_script() {

#ifdef USE_SCRIPT_TCL
  if (!ScriptInterps.contains("tcl")) {
    load_libtcl();

    // create interp
    ScriptInterps["tcl"] = new bd::ScriptInterpTCL;
  }
#endif
  init_script_misc();
  init_script_user();
  return 0;
}

int unload_script() {
  for (const auto& kv : ScriptInterps) {
    auto& si = kv.second;
    delete si;
  }
  ScriptInterps.clear();
  return 1;
}

bd::String script_eval(const bd::String& interp, const bd::String& script) {
  if (!ScriptInterps.contains(interp)) return bd::String();
  ContextNote(interp.c_str(), script.c_str());
  /* XXX: Log errors like in script_bind_callback */
  return ScriptInterps[interp]->eval(script);
}

template void script_link_var(const bd::String& name, bd::String& data, bd::ScriptInterp::link_var_hook_t var_hook_func);

template <typename T>
void script_link_var(const bd::String& name, T& data, bd::ScriptInterp::link_var_hook_t var_hook_func) {
  for (const auto& kv : ScriptInterps) {
    auto& si = kv.second;
    switch (si->type()) {
      // This type hacking is done due to not being able to have templated virtual functions
      case bd::ScriptInterp::SCRIPT_TYPE_TCL:
        ContextNote("TCL", name.c_str());
        static_cast<bd::ScriptInterpTCL*>(si)->linkVar(name, data,
            var_hook_func);
        break;
    }
  }
}

/* vim: set sts=2 sw=2 ts=8 et: */
