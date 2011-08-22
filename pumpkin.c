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
#include "rendertarget.h"

#include "images/pumpkin_skin.h"
#include "pumpkin_skin_tpl.h"

static TPLFile pumpkin_skinTPL;

#include "images/gradient.h"
#include "gradient_tpl.h"

static TPLFile gradientTPL;

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

static void
beam_composite_tev_setup (void)
{
#include "beam-render.inc"
}

static float phase, phase2;

static void *beams_texture;
static void *beams_z_texture;
static void *beams_texture2;

#define BEAMS_TEX_W 640
#define BEAMS_TEX_H 480
#define BEAMS_TEX_TF GX_TF_RGBA8
#define BEAMS_TEX_ZTF GX_TF_Z24X8

static void
pumpkin_init_effect (void *params)
{
  guPerspective (proj, 60, 1.33f, 10.0f, 500.0f);
  phase = 0;
  phase2 = 0;
  TPL_OpenTPLFromMemory (&pumpkin_skinTPL, (void *) pumpkin_skin_tpl,
			 pumpkin_skin_tpl_size);

  TPL_OpenTPLFromMemory (&gradientTPL, (void *) gradient_tpl,
			 gradient_tpl_size);

  beams_texture = memalign (32, GX_GetTexBufferSize (BEAMS_TEX_W, BEAMS_TEX_H,
			    BEAMS_TEX_TF, GX_FALSE, 0));
  beams_z_texture = memalign (32, GX_GetTexBufferSize (BEAMS_TEX_W, BEAMS_TEX_H,
			      BEAMS_TEX_ZTF, GX_FALSE, 0));
  beams_texture2 = memalign (32, GX_GetTexBufferSize (BEAMS_TEX_W, BEAMS_TEX_H,
			     BEAMS_TEX_TF, GX_FALSE, 0));
}

static void
pumpkin_uninit_effect (void *params)
{
  free (beams_texture);
  free (beams_z_texture);
  free (beams_texture2);
}

static void
draw_beams (int shader)
{
  Mtx mvtmp;
  object_loc reflection_loc;
  scene_info reflscene;
  Mtx44 ortho;

  /* "Straight" camera.  */
  scene_set_pos (&reflscene, (guVector) { 0, 0, 0 });
  scene_set_lookat (&reflscene, (guVector) { 5, 0, 0 });
  scene_set_up (&reflscene, (guVector) { 0, 1, 0 });
  scene_update_camera (&reflscene);

  guOrtho (ortho, -1, 1, -1, 1, 1, 15);
  
  object_loc_initialise (&reflection_loc, GX_PNMTX0);
  
  guMtxIdentity (mvtmp);

  scene_update_matrices (&reflscene, &reflection_loc, reflscene.camera, mvtmp,
			 ortho, GX_ORTHOGRAPHIC);

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  switch (shader)
    {
    case 0:
      {
        #include "remap-texchans.inc"
      }
      break;
    
    case 1:
      {
        #include "beam-z-render.inc"
	GX_SetZTexture (GX_ZT_REPLACE, BEAMS_TEX_ZTF, 0);
	/* tevsl doesn't support Z-textures yet!  */
	GX_SetTevOrder (GX_TEVSTAGE3, GX_TEXCOORD0, GX_TEXMAP5, GX_COLORNULL);
        GX_SetZCompLoc (GX_FALSE);
      }
      break;
    }

  GX_SetCullMode (GX_CULL_NONE);

  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT1, 4);

  GX_Position3f32 (3, -1, 1);
  GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (3, -1, -1);
  GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (3, 1, 1);
  GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (3, 1, -1);
  GX_TexCoord2f32 (0, 1);

  GX_End ();
}

static void
pumpkin_display_effect (uint32_t time_offset, void *params, int iparam,
			GXRModeObj *rmode)
{
  Mtx modelview, mvtmp;
  object_loc pumpkin_loc, beam_loc;
  GXTexObj pumpkin_tex_obj;
  GXTexObj beams_tex_obj;
  GXTexObj beams_z_tex_obj;
  GXTexObj beams_tex2_obj;
  GXTexObj gradient_tex_obj;
  unsigned int i;

  TPL_GetTexture (&pumpkin_skinTPL, pumpkin_skin, &pumpkin_tex_obj);
  TPL_GetTexture (&gradientTPL, gradient, &gradient_tex_obj);

  /* We probably don't really need to do this per-frame?  */
  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&pumpkin_tex_obj, GX_TEXMAP0);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  GX_LoadTexObj (get_utility_texture (UTIL_TEX_8BIT_RAMP), GX_TEXMAP1);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);

  GX_InitTexObj (&beams_tex_obj, beams_texture, BEAMS_TEX_W, BEAMS_TEX_H,
		 BEAMS_TEX_TF, GX_CLAMP, GX_CLAMP, GX_FALSE);

  GX_InitTexObj (&beams_z_tex_obj, beams_z_texture, BEAMS_TEX_W, BEAMS_TEX_H,
		 BEAMS_TEX_ZTF, GX_CLAMP, GX_CLAMP, GX_FALSE);

  GX_InitTexObj (&beams_tex2_obj, beams_texture2, BEAMS_TEX_W, BEAMS_TEX_H,
		 BEAMS_TEX_TF, GX_CLAMP, GX_CLAMP, GX_FALSE);

  GX_InitTexObjWrapMode (&gradient_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&gradient_tex_obj, GX_LINEAR, GX_LINEAR);
  GX_LoadTexObj (&gradient_tex_obj, GX_TEXMAP3);

  scene_set_pos (&scene, (guVector) { 100 * cosf (phase), 15 * sinf (phase2),
				      100 * sinf (phase) });
  scene_set_lookat (&scene, (guVector) { 0, 40, 0 });
  
  object_loc_initialise (&pumpkin_loc, GX_PNMTX0);
  object_loc_initialise (&beam_loc, GX_PNMTX0);

  object_set_vertex_depth_mtx (&beam_loc, GX_TEXMTX0);

  scene_update_camera (&scene);

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
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview, proj,
			 GX_PERSPECTIVE);

  /* Render beams to texture.  */
  rendertarget_texture (BEAMS_TEX_W, BEAMS_TEX_H, BEAMS_TEX_TF);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  /* Urgh, what.  */
  /*GX_SetCopyClear ((GXColor) { 1, 1, 1, 1 }, 0x00ffffff);
  GX_CopyTex (beams_texture, GX_TRUE);*/
  
  /* Render beams (to texmap 2).  */

  /* Always update: we want the closer of the back face & front face, if we're
     drawing both.  The dodgy algorithm is this: if we have an opaque object
     in front, we definately draw that.  If the opaque object is behind, we
     can alpha-blend based on the difference between the opaque & transparent
     objects (though that might be overkill).
     This isn't correct if we have *two* transparent regions in front of an
     opaque object, but never mind about that.  */
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetCullMode (GX_CULL_FRONT);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  beam_lighting ();

  /* Add back faces.  */
  guMtxCopy (modelview, mvtmp);
  
  for (i = 0; i < 3; i++)
    {
      scene_update_matrices (&scene, &beam_loc, scene.camera, mvtmp, NULL, 0);

      GX_SetTevKColor (0, (GXColor) { 255, 0, 0, 0 });
      object_set_arrays (&beam_left_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_left_obj, OBJECT_POS, GX_VTXFMT0);

      GX_SetTevKColor (0, (GXColor) { 0, 255, 0, 0 });
      object_set_arrays (&beam_right_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_right_obj, OBJECT_POS, GX_VTXFMT0);

      GX_SetTevKColor (0, (GXColor) { 0, 0, 255, 0 });
      object_set_arrays (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0);

      guMtxTransApply (modelview, mvtmp, 0, 45, 0);
    }

  /* Take away front faces.  */
  GX_SetBlendMode (GX_BM_SUBTRACT, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_BACK);
  
  guMtxCopy (modelview, mvtmp);
  
  for (i = 0; i < 3; i++)
    {
      scene_update_matrices (&scene, &beam_loc, scene.camera, mvtmp, NULL, 0);

      GX_SetTevKColor (0, (GXColor) { 255, 0, 0, 0 });
      object_set_arrays (&beam_left_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_left_obj, OBJECT_POS, GX_VTXFMT0);

      GX_SetTevKColor (0, (GXColor) { 0, 255, 0, 0 });
      object_set_arrays (&beam_right_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_right_obj, OBJECT_POS, GX_VTXFMT0);

      GX_SetTevKColor (0, (GXColor) { 0, 0, 255, 0 });
      object_set_arrays (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0);

      guMtxTransApply (modelview, mvtmp, 0, 45, 0);
    }

    /* Copy the Z buffer for the rendered beams!  */
  GX_SetTexCopyDst (BEAMS_TEX_W, BEAMS_TEX_H, GX_TF_Z24X8, GX_FALSE);
  GX_CopyTex (beams_z_texture, GX_FALSE);

  GX_SetTexCopyDst (BEAMS_TEX_W, BEAMS_TEX_H, GX_TF_RGBA8, GX_FALSE);
  GX_CopyTex (beams_texture, GX_TRUE);

  GX_PixModeSync ();
  
  GX_LoadTexObj (&beams_tex_obj, GX_TEXMAP2);

  /* Remap texture colour channels so we can use all of A,B,G as texture
     coordinates...  */

  rendertarget_texture (BEAMS_TEX_W, BEAMS_TEX_H, BEAMS_TEX_TF);
  GX_SetPixelFmt (GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
  
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_NONE);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  draw_beams (0);
  
  GX_CopyTex (beams_texture2, GX_TRUE);
  GX_PixModeSync ();

  /* Render pumpkins...  */

  rendertarget_screen (rmode);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  GX_SetCullMode (GX_CULL_BACK);

  light_update (scene.camera, &light0);
  pumpkin_lighting ();

  object_set_arrays (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, modelview, proj,
			 GX_PERSPECTIVE);

  object_render (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		 GX_VTXFMT0);
  
  guMtxTransApply (modelview, mvtmp, 0, 45, 0);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, mvtmp, NULL, 0);

  object_render (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp, 0, 45, 0);
  scene_update_matrices (&scene, &pumpkin_loc, scene.camera, mvtmp, NULL,
			 0);

  object_render (&pumpkin_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  
  /* Load indirect texture matrices.  */
  {
    f32 indmtx[2][3];
    
    //GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
    
    /* Texture indices from colours go from 0 to 255: these are interpreted
       exactly as-is for texture coordinates.  We have to scale by 0.5 (giving
       us 0...127) due to the limited range of indirect matrices, then use an
       exponent of 1 to get back to 0...255.  Also we multiply by 4 translating
       from RGB8 to RGBA6, so compensate for that.  */
    
    indmtx[0][0] = 0.5; indmtx[0][1] = 0.0; indmtx[0][2] = 0.0;
    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.0;
    
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, -1);
    
    indmtx[0][0] = 0.0; indmtx[0][1] = 0.5; indmtx[0][2] = 0.0;
    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.0;

    GX_SetIndTexMatrix (GX_ITM_1, indmtx, -1);

    indmtx[0][0] = 0.0; indmtx[0][1] = 0.0; indmtx[0][2] = 0.5;
    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.0;

    GX_SetIndTexMatrix (GX_ITM_2, indmtx, -1);
  }

  GX_LoadTexObj (&beams_tex2_obj, GX_TEXMAP4);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetCullMode (GX_CULL_BACK);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  if (0)
    {
      beam_composite_tev_setup ();

      /* Set up texcoord1 to map to screen-space coordinates.  */
      object_set_screenspace_tex_mtx (&beam_loc, GX_TEXMTX1);
      GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX1);

      /* Draw front faces.  */
      guMtxTransApply (modelview, mvtmp, 0, 45, 0);
      scene_update_matrices (&scene, &beam_loc, scene.camera, mvtmp, proj,
			     GX_PERSPECTIVE);

      object_set_arrays (&beam_left_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_left_obj, OBJECT_POS, GX_VTXFMT0);

      object_set_arrays (&beam_right_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_right_obj, OBJECT_POS, GX_VTXFMT0);

      object_set_arrays (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0);

      /* Draw back faces.  */
      GX_SetCullMode (GX_CULL_FRONT);

      object_set_arrays (&beam_left_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_left_obj, OBJECT_POS, GX_VTXFMT0);

      object_set_arrays (&beam_right_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_right_obj, OBJECT_POS, GX_VTXFMT0);

      object_set_arrays (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0, 0);
      object_render (&beam_mouth_obj, OBJECT_POS, GX_VTXFMT0);
    }
  else
    {
      GX_LoadTexObj (&beams_z_tex_obj, GX_TEXMAP5);
      draw_beams (1);
      GX_SetZTexture (GX_ZT_DISABLE, GX_TF_I4, 0);
      GX_SetZCompLoc (GX_TRUE);
    }

  phase += 0.01;
  phase2 += 0.008;
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
