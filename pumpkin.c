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
#include "scene.h"

#include "images/pumpkin_skin.h"
#include "pumpkin_skin_tpl.h"

static TPLFile pumpkin_skinTPL;

#include "objects/pumpkin.inc"

INIT_OBJECT (pumpkin_obj, pumpkin);

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 proj;
static scene_info scene =
{
  .pos = { 0, 0, -50 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 },
  .camera_dirty = 1
};

static void
pumpkin_lighting (void)
{
  GXLightObj lo0;
  guVector ldir;

#include "pumpkin-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 224, 224, 224, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  /* Orangey!  */
  GX_SetChanAmbColor (GX_COLOR1, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_COLOR1, (GXColor) { 255, 224, 0, 0 });
  GX_SetChanCtrl (GX_COLOR1, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);
}

static float phase, phase2;

static void *pumpkin_skin_txr;

static void
pumpkin_init_effect (void *params)
{
  guPerspective (proj, 60, 1.33f, 10.0f, 500.0f);
  phase = 0;
  phase2 = 0;
  TPL_OpenTPLFromMemory (&pumpkin_skinTPL, (void *) pumpkin_skin_tpl,
			 pumpkin_skin_tpl_size);

  pumpkin_skin_txr = memalign (32, GX_GetTexBufferSize (1024, 512, GX_TF_RGB565,
			       GX_FALSE, 0));
  if (!pumpkin_skin_txr)
    {
      srv_printf ("out of memory!\n");
      exit (1);
    }
}

static void
pumpkin_uninit_effect (void *params)
{
  free (pumpkin_skin_txr);
}

static void
pumpkin_display_effect (uint32_t time_offset, void *params, int iparam,
			GXRModeObj *rmode)
{
  Mtx modelview;
  object_loc pumpkin_loc;
  GXTexObj pumpkin_tex_obj;

  TPL_GetTexture (&pumpkin_skinTPL, pumpkin_skin, &pumpkin_tex_obj);

  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&pumpkin_tex_obj, GX_TEXMAP0);

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  scene_set_pos (&scene, (guVector) { 100 * cosf (phase), 15 * sinf (phase2),
				      100 * sinf (phase) });
  scene_set_lookat (&scene, (guVector) { 0, 40, 0 });
  
  object_loc_initialise (&pumpkin_loc, GX_PNMTX0);

  scene_update_camera (&scene);

  GX_LoadProjectionMtx (proj, GX_PERSPECTIVE);
  
  guMtxIdentity (modelview);
  guMtxScaleApply (modelview, modelview, 30, 30, 30);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview);
  
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  GX_SetCullMode (GX_CULL_BACK);

  light_update (scene.camera, &light0);
  pumpkin_lighting ();

  object_set_arrays (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);
  
  object_render (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		 GX_VTXFMT0);
  
  guMtxTransApply (modelview, modelview, 0, 45, 0);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview);

  object_render (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  guMtxTransApply (modelview, modelview, 0, 45, 0);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview);

  object_render (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  phase += 0.04;
  phase2 += 0.03;
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
