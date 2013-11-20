#ifndef _SCRIPT_H
#define _SCRIPT_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#include <bdlib/src/HashTable.h>
#include <bdlib/src/ScriptInterp.h>
#include <bdlib/src/ScriptInterpTCL.h>

extern bd::HashTable< bd::String, bd::ScriptInterp* > ScriptInterps;
bd::String script_eval(const bd::String& interp, const bd::String& script);

template <typename T>
void script_link_var(const bd::String& name, T& data, bd::ScriptInterp::link_var_hook = nullptr);

int init_script();
int unload_script();

template<typename ReturnType, typename... Params>
void script_add_command(const bd::String& cmdName, ReturnType(*callback)(Params...), size_t minParams = sizeof...(Params)) {
  bd::Array<bd::String> interps(ScriptInterps.keys());

  for (auto key : interps) {
    switch (ScriptInterps[key]->type()) {
      // This type hacking is done due to not being able to have templated virtual functions
      case bd::ScriptInterp::SCRIPT_TYPE_TCL:
        bd::ScriptInterp::createCommand(*static_cast<bd::ScriptInterpTCL*>(ScriptInterps[key]), cmdName, callback, minParams);
        break;
    }
  }
}

#endif /* !_SCRIPT_H */
