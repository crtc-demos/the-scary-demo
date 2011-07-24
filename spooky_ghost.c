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

#include "objects/spooky-ghost.inc"

INIT_OBJECT (spooky_ghost_obj, spooky_ghost);

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 perspmat;
static Mtx viewmat;
static guVector pos = {0, 0, 30};
static guVector up = {0, 1, 0};
static guVector lookat = {0, 0, 0};
static float deg = 0.0;
static float deg2 = 0.0;

static void
specular_lighting_1light (void)
{
  GXLightObj lo0;
  guVector ldir;

#include "cube-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

static void
spooky_ghost_init_effect (void *params)
{
  guPerspective (perspmat, 60, 1.33f, 10.0f, 300.0f);
  guLookAt (viewmat, &pos, &up, &lookat);
}

static void
spooky_ghost_display_effect (uint32_t time_offset, void *params, int iparam,
			     GXRModeObj *rmode)
{
  Mtx modelView, mvi, mvitmp, rotmtx, rotmtx2, mvtmp;
  guVector axis = {0, 1, 0};
  guVector axis2 = {0, 0, 1};

  GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  light_update (viewmat, &light0);

  specular_lighting_1light ();

  object_set_arrays (&spooky_ghost_obj, GX_VTXFMT0);

  guMtxIdentity (modelView);
  guMtxRotAxisDeg (rotmtx, &axis, deg);
  guMtxRotAxisDeg (rotmtx2, &axis2, deg2);

  guMtxConcat (rotmtx, mvtmp, mvtmp);
  guMtxConcat (rotmtx2, mvtmp, mvtmp);
  guMtxConcat (viewmat, mvtmp, mvtmp);
  
  GX_LoadPosMtxImm (mvtmp, GX_PNMTX0);
  guMtxInverse (mvtmp, mvitmp);
  guMtxTranspose (mvitmp, mvi);
  GX_LoadNrmMtxImm (mvi, GX_PNMTX0);
  
  object_render (&spooky_ghost_obj, GX_VTXFMT0);
  
  deg++;
  deg2 += 0.5;
}

effect_methods spooky_ghost_methods =
{
  .preinit_assets = NULL,
  .init_effect = &spooky_ghost_init_effect,
  .prepare_frame = NULL,
  .display_effect = &spooky_ghost_display_effect,
  .uninit_effect = NULL,
  .finalize = NULL
};
