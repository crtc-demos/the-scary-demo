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
#include "shader.h"
#include "screenspace.h"
#include "matrixutil.h"

#include "images/pumpkin_skin.h"
#include "pumpkin_skin_tpl.h"

static TPLFile pumpkin_skinTPL;

#include "images/gradient.h"
#include "gradient_tpl.h"

static TPLFile gradientTPL;

#include "objects/pumpkin.inc"
#include "objects/carved-pumpkin.inc"
#include "objects/beam-left.inc"
#include "objects/beam-right.inc"
#include "objects/beam-mouth.inc"

INIT_OBJECT (pumpkin_obj, pumpkin);
INIT_OBJECT (carved_pumpkin_obj, carved_pumpkin);
INIT_OBJECT (beam_left_obj, beam_left);
INIT_OBJECT (beam_right_obj, beam_right);
INIT_OBJECT (beam_mouth_obj, beam_mouth);

#include "cam-path.h"
#include "objects/pumpkin-track.xyz"

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

pumpkin_data pumpkin_data_0;

static void
pumpkin_lighting (void *dummy)
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
beam_lighting (void *dummy)
{
#include "beam-front-or-back.inc"
}

#if 0
static void
beam_composite_tev_setup (void)
{
#include "beam-render.inc"
}
#endif

/*static float phase, phase2;*/

#define BEAMS_TEX_W 640
#define BEAMS_TEX_H 480
#define BEAMS_TEX_TF GX_TF_RGBA8
#define BEAMS_TEX_ZTF GX_TF_Z24X8

static void
beam_z_render (void *dummy)
{
#include "beam-z-render.inc"
}

static void
pumpkin_init_effect (void *params, backbuffer_info *bbuf)
{
  pumpkin_data *pdata = (pumpkin_data *) params;
  int i;

  guPerspective (proj, 60, 1.33f, 10.0f, 500.0f);
  /*phase = 0;
  phase2 = 0;*/
  TPL_OpenTPLFromMemory (&pumpkin_skinTPL, (void *) pumpkin_skin_tpl,
			 pumpkin_skin_tpl_size);

  TPL_OpenTPLFromMemory (&gradientTPL, (void *) gradient_tpl,
			 gradient_tpl_size);

  pdata->beams_texture_gb = memalign (32, GX_GetTexBufferSize (BEAMS_TEX_W,
			      BEAMS_TEX_H, GX_TF_IA8, GX_FALSE, 0));
  pdata->beams_texture_r = memalign (32, GX_GetTexBufferSize (BEAMS_TEX_W,
			      BEAMS_TEX_H, GX_TF_I8, GX_FALSE, 0));
  pdata->beams_z_texture = memalign (32, GX_GetTexBufferSize (BEAMS_TEX_W,
			      BEAMS_TEX_H, BEAMS_TEX_ZTF, GX_FALSE, 0));

  assert (pdata->beams_texture_gb && pdata->beams_texture_r
	  && pdata->beams_z_texture);

  TPL_GetTexture (&pumpkin_skinTPL, pumpkin_skin, &pdata->pumpkin_tex_obj);

  pdata->pumpkin_lighting_shader = create_shader (&pumpkin_lighting, NULL);
  shader_append_texmap (pdata->pumpkin_lighting_shader, &pdata->pumpkin_tex_obj,
			GX_TEXMAP0);
  shader_append_texcoordgen (pdata->pumpkin_lighting_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  /* Set up shader for beam-drawing pre-pass.  */
  pdata->beam_lighting_shader = create_shader (&beam_lighting, NULL);
  shader_append_texmap (pdata->beam_lighting_shader,
			get_utility_texture (UTIL_TEX_8BIT_RAMP), GX_TEXMAP1);
  shader_append_texcoordgen (pdata->beam_lighting_shader, GX_TEXCOORD1,
			     GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);

  /* Set up shader for beam-drawing post-pass.  */
  TPL_GetTexture (&gradientTPL, gradient, &pdata->gradient_tex_obj);

  GX_InitTexObjWrapMode (&pdata->gradient_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&pdata->gradient_tex_obj, GX_LINEAR, GX_LINEAR);

  GX_InitTexObj (&pdata->beams_gb_tex_obj, pdata->beams_texture_gb,
		 BEAMS_TEX_W, BEAMS_TEX_H, GX_TF_IA8, GX_CLAMP, GX_CLAMP,
		 GX_FALSE);

  GX_InitTexObj (&pdata->beams_r_tex_obj, pdata->beams_texture_r, BEAMS_TEX_W,
		 BEAMS_TEX_H, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);

  GX_InitTexObj (&pdata->beams_z_tex_obj, pdata->beams_z_texture, BEAMS_TEX_W,
		 BEAMS_TEX_H, BEAMS_TEX_ZTF, GX_CLAMP, GX_CLAMP, GX_FALSE);

  pdata->beam_z_render_shader = create_shader (&beam_z_render, NULL);
  shader_append_texmap (pdata->beam_z_render_shader, &pdata->beams_z_tex_obj,
			GX_TEXMAP2);
  shader_append_texmap (pdata->beam_z_render_shader, &pdata->gradient_tex_obj,
			GX_TEXMAP3);
  shader_append_texmap (pdata->beam_z_render_shader, &pdata->beams_gb_tex_obj,
			GX_TEXMAP4);
  shader_append_texmap (pdata->beam_z_render_shader, &pdata->beams_r_tex_obj,
			GX_TEXMAP5);
  shader_append_texcoordgen (pdata->beam_z_render_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  object_loc_initialise (&pdata->pumpkin_loc, GX_PNMTX0);
  object_loc_initialise (&pdata->beam_loc, GX_PNMTX0);

  object_set_vertex_depth_matrix (&pdata->beam_loc, GX_TEXMTX0);
  
  assert (pdata->pumpkin_lighting_shader && pdata->beam_lighting_shader
	  && pdata->beam_z_render_shader);

  for (i = 0; i < 3; i++)
    {
      pdata->vpos[i] = 250.0;
      pdata->spd[i] = 0.0;
    }
  pdata->central_pumpkin_rotation = M_PI;
}

static void
pumpkin_uninit_effect (void *params, backbuffer_info *bbuf)
{
  /*spline_tracking_obj *sto =
    (spline_tracking_obj *) cube_tracking_scene.follow_path;*/
  pumpkin_data *pdata = (pumpkin_data *) params;

  free (pdata->beams_texture_gb);
  free (pdata->beams_texture_r);
  free (pdata->beams_z_texture);
  /*free_spline_data (&sto->spline);*/
  free_shader (pdata->pumpkin_lighting_shader);
  free_shader (pdata->beam_z_render_shader);
  free_shader (pdata->beam_lighting_shader);
}

static display_target
pumpkin_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  pumpkin_data *pdata = (pumpkin_data *) params;
  Mtx mvtmp, rot/*, sep_scale*/;

  //guMtxScale (sep_scale, 30, 30, 30);

  /* We probably don't really need to do this per-frame?  */
  GX_InvalidateTexAll ();

#if 1
  {
    Mtx path_mtx;
    guMtxScale (path_mtx, 30, 30, 30);
    matrixutil_swap_yz (path_mtx, path_mtx);
    cam_path_follow (&scene, path_mtx, &pumpkin_track,
		     (float) time_offset / 10000.0);
  }
#else
  scene_set_pos (&scene, (guVector) { 100 * cosf (phase), 15 * sinf (phase2),
				      100 * sinf (phase) });
  scene_set_lookat (&scene, (guVector) { 0, 40, 0 });
#endif
  
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

  guMtxScale (pdata->modelview, 30, 30, 30);
  object_set_matrices (&scene, &pdata->pumpkin_loc, scene.camera,
		       pdata->modelview, NULL, proj, GX_PERSPECTIVE);

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

  shader_load (pdata->beam_lighting_shader);

  //GX_SetFog (GX_FOG_PERSP_EXP2, 130, 300, 10, 500, (GXColor) { 0, 0, 0, 0 });

  /* Add back faces.  */
  guMtxTransApply (pdata->modelview, mvtmp, 0, 45, 0);
  guMtxRotRad (rot, 'y', pdata->central_pumpkin_rotation);
  guMtxConcat (mvtmp, rot, mvtmp);
  
  object_set_matrices (&scene, &pdata->beam_loc, scene.camera, mvtmp, NULL,
		       NULL, 0);

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

  /* Copy the Z buffer for the rendered beams!  */
  GX_SetTexCopyDst (BEAMS_TEX_W, BEAMS_TEX_H, GX_TF_Z24X8, GX_FALSE);
  GX_CopyTex (pdata->beams_z_texture, GX_FALSE);

  /* Copy green & blue channels to intensity & alpha channels of
     beams_texture_gb.  */
  GX_SetTexCopyDst (BEAMS_TEX_W, BEAMS_TEX_H, GX_CTF_GB8, GX_FALSE);
  GX_CopyTex (pdata->beams_texture_gb, GX_FALSE);

  /* Copy red channel to intensity channel of beams_texture_r.  */
  GX_SetTexCopyDst (BEAMS_TEX_W, BEAMS_TEX_H, GX_CTF_R8, GX_FALSE);
  GX_CopyTex (pdata->beams_texture_r, GX_TRUE);

  GX_PixModeSync ();
  
  return MAIN_BUFFER;
}

static void
pumpkin_display_effect (uint32_t time_offset, void *params, int iparam)
{
  pumpkin_data *pdata = (pumpkin_data *) params;
  Mtx mvtmp, rot, mvtmp2/*, sep_scale*/;
  int i;

  /*guMtxScale (sep_scale, 30, 30, 30);*/

  /* Render pumpkins...  */

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  GX_SetCullMode (GX_CULL_BACK);

  light_update (scene.camera, &light0);
  shader_load (pdata->pumpkin_lighting_shader);

  if (time_offset < 5000)
    {
      object_set_arrays (&pumpkin_obj,
			 OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0,
			 GX_VA_TEX0);

      for (i = 0; i < 3; i++)
        {
	  guMtxTransApply (pdata->modelview, mvtmp, 0, pdata->vpos[i], 0);
	  object_set_matrices (&scene, &pdata->pumpkin_loc, scene.camera,
			       mvtmp, NULL, proj, GX_PERSPECTIVE);
	  object_render (&pumpkin_obj,
			 OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
			 GX_VTXFMT0);
	}

      for (i = 0; i < 3; i++)
        pdata->vpos[i] += pdata->spd[i];

      pdata->spd[0] -= 0.2;
      if (time_offset > 1000)
        pdata->spd[1] -= 0.2;
      if (time_offset > 2000)
        pdata->spd[2] -= 0.2;

      for (i = 0; i < 3; i++)
        if (pdata->vpos[i] < i * 45.0)
	  {
	    pdata->vpos[i] = i * 45.0;
	    pdata->spd[i] = 0.0;
	  }
    }
  else
    {
      object_set_arrays (&pumpkin_obj,
			 OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0,
			 GX_VA_TEX0);
      object_set_matrices (&scene, &pdata->pumpkin_loc, scene.camera,
			   pdata->modelview, NULL, proj, GX_PERSPECTIVE);
      object_render (&pumpkin_obj,
		     OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0);

      guMtxTransApply (pdata->modelview, mvtmp, 0, 45, 0);
      guMtxRotRad (rot, 'y', pdata->central_pumpkin_rotation);
      guMtxConcat (mvtmp, rot, mvtmp2);

      object_set_matrices (&scene, &pdata->pumpkin_loc, scene.camera, mvtmp2,
			   NULL, NULL, 0);
      object_set_arrays (&carved_pumpkin_obj,
			 OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0,
			 GX_VA_TEX0);
      object_render (&carved_pumpkin_obj,
		     OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0);

      guMtxTransApply (mvtmp, mvtmp, 0, 45, 0);

      object_set_matrices (&scene, &pdata->pumpkin_loc, scene.camera, mvtmp,
			   NULL, NULL, 0);
      object_set_arrays (&pumpkin_obj,
			 OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0,
			 GX_VA_TEX0);
      object_render (&pumpkin_obj,
		     OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD, GX_VTXFMT0);

      if (time_offset > 6000 && (rand () & 8))
        {
	  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
	  GX_SetColorUpdate (GX_TRUE);
	  GX_SetAlphaUpdate (GX_FALSE);

	  /* Load indirect texture matrices.  */
	  {
	    f32 indmtx[2][3];

	    //GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);

	    /* Texture indices from colours go from 0 to 255: these are interpreted
	       exactly as-is for texture coordinates.  We have to scale by 0.5
	       (giving us 0...127) due to the limited range of indirect matrices,
	       then use an exponent of 1 to get back to 0...255.  Also we multiply
	       by 4 translating from RGB8 to RGBA6, so compensate for that (not any
	       more).  */

	    indmtx[0][0] = 0.5; indmtx[0][1] = 0.0; indmtx[0][2] = 0.0;
	    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.0;

	    GX_SetIndTexMatrix (GX_ITM_0, indmtx, 1);

	    indmtx[0][0] = 0.0; indmtx[0][1] = 0.5; indmtx[0][2] = 0.0;
	    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.0;

	    GX_SetIndTexMatrix (GX_ITM_1, indmtx, 1);

	    indmtx[0][0] = 0.0; indmtx[0][1] = 0.0; indmtx[0][2] = 0.5;
	    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.0;

	    GX_SetIndTexMatrix (GX_ITM_2, indmtx, 1);
	  }

	  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
	  GX_SetCullMode (GX_CULL_BACK);
	  GX_SetColorUpdate (GX_TRUE);
	  GX_SetAlphaUpdate (GX_FALSE);

	  screenspace_rect (pdata->beam_z_render_shader, GX_VTXFMT1, 0);
	}

      if (pdata->central_pumpkin_rotation <= 0)
        pdata->central_pumpkin_rotation = 0;
      else
        pdata->central_pumpkin_rotation -= M_PI / 20.0;
    }

  /*phase += 0.01;
  phase2 += 0.008;*/
}

effect_methods pumpkin_methods =
{
  .preinit_assets = NULL,
  .init_effect = &pumpkin_init_effect,
  .prepare_frame = &pumpkin_prepare_frame,
  .display_effect = &pumpkin_display_effect,
  .uninit_effect = &pumpkin_uninit_effect,
  .finalize = NULL
};
