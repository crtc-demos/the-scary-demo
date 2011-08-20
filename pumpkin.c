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
#include "utility-texture.h"

#include "images/pumpkin_skin.h"
#include "pumpkin_skin_tpl.h"

static TPLFile pumpkin_skinTPL;

#include "objects/pumpkin.inc"
#include "objects/beam-left.inc"
#include "objects/beam-right.inc"
#include "objects/beam-mouth.inc"

INIT_OBJECT (pumpkin_obj, pumpkin);
INIT_OBJECT (beam_left_obj, beam_left);
INIT_OBJECT (beam_right_obj, beam_right);
INIT_OBJECT (beam_mouth_obj, beam_mouth);

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

static void
beam_lighting (void)
{
#include "beam-front-or-back.inc"
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
  object_loc pumpkin_loc, beam_loc;
  GXTexObj pumpkin_tex_obj;

  TPL_GetTexture (&pumpkin_skinTPL, pumpkin_skin, &pumpkin_tex_obj);

  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&pumpkin_tex_obj, GX_TEXMAP0);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  GX_LoadTexObj (get_utility_texture (UTIL_TEX_8BIT_RAMP), GX_TEXMAP1);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);
  
  scene_set_pos (&scene, (guVector) { 100 * cosf (phase), 15 * sinf (phase2),
				      100 * sinf (phase) });
  scene_set_lookat (&scene, (guVector) { 0, 40, 0 });
  
  object_loc_initialise (&pumpkin_loc, GX_PNMTX0);
  object_loc_initialise (&beam_loc, GX_PNMTX0);

  object_set_vertex_depth_mtx (&beam_loc, GX_TEXMTX0);

  scene_update_camera (&scene);

  GX_LoadProjectionMtx (proj, GX_PERSPECTIVE);

  /* Put this somewhere nicer!  We need to do the same thing for shadow
     mapping.  */
  {
    float near, far, range, tscale;
    Mtx dp = { { 0.0, 0.0, 0.0, 0.0 },
               { 0.0, 0.0, 0.0, 0.0 },
	       { 0.0, 0.0, 0.0, 0.0 } };
    
    near = 10.0f;
    far = 300.0f;
    
    range = far - near;
    tscale = 16.0f;
    
    /* This is a "perspective" frustum, mapping depth in scene (of vertices)
       to a ramp texture.  */
#if 0
    /* Why doesn't this work nicely?  */
    guMtxRowCol (dp, 0, 2) = far / range;
    guMtxRowCol (dp, 0, 3) = far * near / range;
    guMtxRowCol (dp, 1, 2) = guMtxRowCol (dp, 0, 2) * tscale;
    guMtxRowCol (dp, 1, 3) = guMtxRowCol (dp, 0, 3) * tscale;
    guMtxRowCol (dp, 2, 2) = 1.0f;
#else
    guMtxRowCol (dp, 0, 2) = -1.0f / range;
    guMtxRowCol (dp, 0, 3) = -near / range;
    guMtxRowCol (dp, 1, 2) = guMtxRowCol (dp, 0, 2) * tscale;
    guMtxRowCol (dp, 1, 3) = guMtxRowCol (dp, 0, 3) * tscale;
    guMtxRowCol (dp, 2, 3) = 1.0f;
#endif
    
    guMtxCopy (dp, scene.depth_ramp_lookup);
  }
  
  guMtxIdentity (modelview);
  guMtxScaleApply (modelview, modelview, 30, 30, 30);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  GX_SetCullMode (GX_CULL_BACK);

  /* Render pumpkins...  */

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

  /* Render beams... */

  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetCullMode (GX_CULL_FRONT);

  beam_lighting ();

  /* Add back faces.  */
  scene_update_matrices (&scene, &beam_loc, scene.camera, modelview);

  GX_SetTevKColor (0, (GXColor) { 255, 0, 0, 0 });
  object_set_arrays (&beam_left_obj, OBJECT_POS, GX_VTXFMT0, 0);
  object_render (&beam_left_obj, OBJECT_POS, GX_VTXFMT0);

  GX_SetTevKColor (0, (GXColor) { 0, 255, 0, 0 });
  object_set_arrays (&beam_right_obj, OBJECT_POS, GX_VTXFMT0, 0);
  object_render (&beam_right_obj, OBJECT_POS, GX_VTXFMT0);

  GX_SetTevKColor (0, (GXColor) { 0, 0, 255, 0 });
  object_set_arrays (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0, 0);
  object_render (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0);

  /* Take away front faces.  */
  GX_SetBlendMode (GX_BM_SUBTRACT, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_BACK);
  
  GX_SetTevKColor (0, (GXColor) { 255, 0, 0, 0 });
  object_set_arrays (&beam_left_obj, OBJECT_POS, GX_VTXFMT0, 0);
  object_render (&beam_left_obj, OBJECT_POS, GX_VTXFMT0);

  GX_SetTevKColor (0, (GXColor) { 0, 255, 0, 0 });
  object_set_arrays (&beam_right_obj, OBJECT_POS, GX_VTXFMT0, 0);
  object_render (&beam_right_obj, OBJECT_POS, GX_VTXFMT0);

  GX_SetTevKColor (0, (GXColor) { 0, 0, 255, 0 });
  object_set_arrays (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0, 0);
  object_render (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0);

  /* Render another pumpkin...  */

  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_BACK);

  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview);

  pumpkin_lighting ();

  guMtxTransApply (modelview, modelview, 0, 45, 0);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview);

  object_set_arrays (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);
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
