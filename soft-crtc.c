#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

#include "timing.h"
#include "soft-crtc.h"
#include "light.h"
#include "object.h"
#include "server.h"

#include "objects/softcube.inc"

INIT_OBJECT (softcube_obj, softcube);

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx viewmat, perspmat;
static guVector pos = {0, 0, 30};
static guVector up = {0, 1, 0};
static guVector lookat = {0, 0, 0};
static float deg = 0.0;
static float deg2 = 0.0;

static void
specular_lighting_1light (void)
{
  GXLightObj lo;
  guVector ldir;

#include "tube-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPEC);

  GX_SetChanCtrl (GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
		  GX_DF_CLAMP, GX_AF_NONE);

  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  /* Light 0: use for both specular and diffuse lighting.  */
  GX_InitSpecularDir (&lo, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo, 64);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

static void
soft_crtc_init_effect (void *params)
{
  guPerspective (perspmat, 60, 1.33f, 10.0f, 300.0f);
  guLookAt (viewmat, &pos, &up, &lookat);
}

static void
soft_crtc_display_effect (uint32_t time_offset, void *params, int iparam,
			  GXRModeObj *rmode)
{
  Mtx modelView, mvi, mvitmp, rotmtx, rotmtx2;
  guVector axis = {0, 1, 0};
  guVector axis2 = {0, 0, 1};
  int i, j;
  static const char logo[7][7] =
    { { ' ', 'x', 'x', ' ', ' ', 'x', 'x' },
      { 'x', ' ', ' ', ' ', 'x', ' ', ' ' },
      { 'x', 'x', 'x', ' ', 'x', ' ', ' ' },
      { ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
      { 'x', 'x', 'x', ' ', ' ', 'x', 'x' },
      { ' ', 'x', ' ', ' ', 'x', ' ', ' ' },
      { ' ', 'x', ' ', ' ', 'x', 'x', 'x' } };

  GX_SetCullMode (GX_CULL_BACK);
  GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
  GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
  
  GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  guVecMultiply (viewmat, &light0.pos, &light0.tpos);
  guVecMultiply (viewmat, &light0.lookat, &light0.tlookat);

  specular_lighting_1light ();
  
  guMtxIdentity (modelView);
  guMtxRotAxisDeg (rotmtx, &axis, deg);
  guMtxRotAxisDeg (rotmtx2, &axis2, deg2);
  
  object_set_arrays (&softcube_obj, GX_VTXFMT0);
  
  for (i = -3; i <= 3; i++)
    for (j = -3; j <= 3; j++)
      {
        if (logo[3 - i][j + 3] == 'x')
	  {
            Mtx mvtmp;

	    guMtxTransApply (modelView, mvtmp, i * 2.2, j * 2.2, 0.0);

	    guMtxScaleApply (mvtmp, mvtmp, 2.0, 2.0, 2.0);

	    guMtxConcat (rotmtx, mvtmp, mvtmp);
	    guMtxConcat (rotmtx2, mvtmp, mvtmp);

	    guMtxConcat (viewmat, mvtmp, mvtmp);

	    GX_LoadPosMtxImm (mvtmp, GX_PNMTX0);
	    guMtxInverse (mvtmp, mvitmp);
	    guMtxTranspose (mvitmp, mvi);
	    GX_LoadNrmMtxImm (mvi, GX_PNMTX0);

	    object_render (&softcube_obj, GX_VTXFMT0);
	  }
      }
  
  deg++;
  deg2 += 0.5;
}

effect_methods soft_crtc_methods =
{
  .preinit_assets = NULL,
  .init_effect = &soft_crtc_init_effect,
  .prepare_frame = NULL,
  .display_effect = &soft_crtc_display_effect,
  .uninit_effect = NULL,
  .finalize = NULL
};
