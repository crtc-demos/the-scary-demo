#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

#include "timing.h"
#include "soft-crtc.h"
#include "light.h"

effect_methods soft_crtc_methods =
{
  .preinit_assets = NULL,
  .init_effect = NULL,
  .prepare_frame = NULL,
  .display_effect = NULL,
  .uninit_effect = NULL,
  .finalize = NULL
};
