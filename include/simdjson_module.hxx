#pragma once

// GCC 16 treats simdjson's use of anonymous-namespace types inside templates
// as a hard module error when included in a global module fragment.  This local
// copy patches the anonymous namespace containing escape_sequence (used by the
// internal formatter) into a named namespace so it no longer triggers
// -Wexpose-global-module-tu-local.
#include "simdjson.h"
