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
 * tcl.c -- handles:
 *   libtcl handling
 *
 */


#include "common.h"
#include "main.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/Array.h>

#include "libtcl.h"
#include ".defs/libtcl_defs.cc"

void *libtcl_handle = NULL;
static bd::Array<bd::String> my_symbols;

static int load_symbols(void *handle) {
#ifdef USE_SCRIPT_TCL
  const char *dlsym_error = NULL;

  DLSYM_GLOBAL(handle, Tcl_Eval);
  DLSYM_GLOBAL(handle, Tcl_GetStringResult);
  DLSYM_GLOBAL(handle, Tcl_DeleteInterp);
  DLSYM_GLOBAL(handle, Tcl_CreateCommand);
  DLSYM_GLOBAL(handle, Tcl_CreateInterp);
  DLSYM_GLOBAL(handle, Tcl_FindExecutable);
  DLSYM_GLOBAL(handle, Tcl_Init);
#endif

  return 0;
}

int load_libtcl() {
#ifndef USE_SCRIPT_TCL
  sdprintf("Not compiled with TCL support");
  return 1;
#endif

  if (libtcl_handle) {
    return 0;
  }

  bd::Array<bd::String> libs_list(bd::String("libtcl.so libtcl83.so libtcl8.3.so libtcl84.so libtcl8.4.so libtcl85.so libtcl8.5.so libtcl86.so libtcl8.6.so").split(' '));

  for (size_t i = 0; i < libs_list.length(); ++i) {
    dlerror(); // Clear Errors
    libtcl_handle = dlopen(bd::String(libs_list[i]).c_str(), RTLD_LAZY);
    if (libtcl_handle) break;
  }
  if (!libtcl_handle) {
    sdprintf("Unable to find libtcl");
    return 1;
  }

  if (load_symbols(libtcl_handle)) {
    fprintf(stderr, STR("Missing symbols for libtcl (likely too old)\n"));
    return(1);
  }

  return 0;
}

int unload_libtcl() {
  if (libtcl_handle) {
    // Cleanup symbol table
    for (size_t i = 0; i < my_symbols.length(); ++i) {
      dl_symbol_table.remove(my_symbols[i]);
      static_cast<bd::String>(my_symbols[i]).clear();
    }
    my_symbols.clear();

    dlclose(libtcl_handle);
    libtcl_handle = NULL;
    return 0;
  }
  return 1;
}

/* vim: set sts=2 sw=2 ts=8 et: */
