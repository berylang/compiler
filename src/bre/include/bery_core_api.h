#pragma once


/*

_core API

these are exact symbol names code generation emits extern calls, 
since bery_print_* names matches BRE internals directl, this header just 
documents the full extern "C" surface code generation depends on.

thus no wrappers needed as code generation declares and calls these symbols directly.

*/


#include "bery_native.h"
#include "bery_string.h"
#include "bery_array.h"
#include "bery_alloc.h"
#include "bery_gc.h"
#include "bery_gc_roots.h"
#include "bery_runtime.h"