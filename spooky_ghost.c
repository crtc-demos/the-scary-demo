#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "spooky_ghost.h"
#include "light.h"
#include "object.h"
#include "server.h"

#include "objects/spooky_ghost.inc"

INIT_OBJECT (spooky_ghost_obj, spooky_ghost);

effect_methods spooky_ghost_methods =
{
  .preinit_assets = NULL,
  .init_effect = NULL,
  .prepare_frame = NULL,
  .display_effect = NULL,
  .uninit_effect = NULL,
  .finalize = NULL
};
