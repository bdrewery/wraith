#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>

#ifdef USE_SCRIPT_TCL
#include <tcl.h>

#define STDVAR (ClientData cd, Tcl_Interp *interp, int argc, const char *argv[])

extern Tcl_Interp *global_interp;
bd::String tcl_eval(const bd::String&);
#endif

int init_script();
int unload_script();


#endif /* !_SCRIPT_H */
