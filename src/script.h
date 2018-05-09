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

struct script_callback {
  public:
    bd::ScriptCallbackerPtr callback_command;
    const bd::String mask;
    const void *cdata;
    script_callback() = delete;
    script_callback(bd::ScriptCallbackerPtr _callback_command,
        bd::String _mask, void *_cdata = nullptr) :
          callback_command(_callback_command), mask(_mask), cdata(_cdata) {};
};

template <typename T>
void script_link_var(const bd::String& name, T& data, bd::ScriptInterp::link_var_hook_t = nullptr);

int init_script();
int unload_script();

template<typename ReturnType, typename... Params>
void script_add_command(const bd::String& cmdName, ReturnType(*callback)(Params...), const char* usage = nullptr, size_t minParams = sizeof...(Params)) {
  bd::Array<bd::String> interps(ScriptInterps.keys());

  for (auto key : interps) {
    switch (ScriptInterps[key]->type()) {
      // This type hacking is done due to not being able to have templated virtual functions
      case bd::ScriptInterp::SCRIPT_TYPE_TCL:
        bd::ScriptInterp::createCommand(*static_cast<bd::ScriptInterpTCL*>(ScriptInterps[key]), cmdName, callback, usage, minParams);
        break;
    }
  }
}

#endif /* !_SCRIPT_H */