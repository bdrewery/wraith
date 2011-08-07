#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/ScriptInterp.h>

extern bd::HashTable< bd::String, bd::ScriptInterp* > ScriptInterps;
bd::String script_eval(const bd::String& interp, const bd::String& script);

typedef struct {
  const char *name;
  bd::ScriptInterp::script_cmd_handler_t callback;
} script_cmd_t;


int init_script();
int unload_script();
void script_add_commands(script_cmd_t* cmds);

#endif /* !_SCRIPT_H */
