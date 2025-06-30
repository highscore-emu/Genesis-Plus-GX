#pragma once

#include <libhighscore.h>

G_BEGIN_DECLS

#define GENESIS_PLUS_GX_TYPE_CORE (genesis_plus_gx_core_get_type())

G_DECLARE_FINAL_TYPE (GenesisPlusGXCore, genesis_plus_gx_core, GENESIS_PLUS_GX, CORE, HsCore)

G_MODULE_EXPORT GType hs_get_core_type (void);

G_END_DECLS