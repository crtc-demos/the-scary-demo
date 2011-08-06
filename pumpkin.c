#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "pumpkin.h"
#include "light.h"
#include "object.h"
#include "server.h"

#include "objects/pumpkin.inc"

INIT_OBJECT (pumpkin_obj, pumpkin);

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static void
pumpkin_init_effect (void *params)
{
}

static void
pumpkin_uninit_effect (void *params)
{
}

static void
pumpkin_display_effect (uint32_t time_offset, void *params, int iparam,
			GXRModeObj *rmode)
{

}

effect_methods pumpkin_methods =
{
  .preinit_assets = NULL,
  .init_effect = &pumpkin_init_effect,
  .prepare_frame = NULL,
  .display_effect = &pumpkin_display_effect,
  .uninit_effect = &pumpkin_uninit_effect,
  .finalize = NULL
};
