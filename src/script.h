#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/ScriptInterp.h>

#ifdef USE_SCRIPT_TCL

extern bd::HashTable< bd::String, bd::ScriptInterp* > ScriptInterps;
bd::String script_eval(const bd::String& interp, const bd::String& script);
#endif

int init_script();
int unload_script();


#endif /* !_SCRIPT_H */
