#ifndef _LIBTCL_H
#define _LIBTCL_H

#include "common.h"
#include "dl.h"
#include <bdlib/src/String.h>
#ifdef USE_SCRIPT_TCL

#include ".defs/libtcl_pre.h"

#include <tcl.h>

#include ".defs/libtcl_post.h"


#endif

int load_libtcl();
int unload_libtcl();


#endif /* !_LIBTCL_H */
