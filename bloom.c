#include <ogcsys.h>
#include <gccore.h>
#include <math.h>
#include <stdlib.h>
#include <malloc.h>

#include "bloom.h"
#include "light.h"
#include "object.h"
#include "server.h"
#include "scene.h"
#include "rendertarget.h"
#include "shadow.h"
#include "screenspace.h"

#include "objects/knot.inc"

/* This is loaded/initialised elsewhere, we don't need to do it again.  */
//extern object_info softcube_obj;
INIT_OBJECT (knot_obj, knot);

static light_info light0 =
{
  .pos = { 40, 10, 10 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static light_info light1 =
{
  .pos = { -20, -20, 20 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 proj;

static scene_info scene =
{
  .pos = { 0, 0, 50 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 },
  .camera_dirty = 1
};

bloom_data bloom_data_0;

#define STAGE1_W 640
#define STAGE1_H 480
#define STAGE1_FMT GX_TF_RGBA8

#define STAGE2_W 320
#define STAGE2_H 240
#define STAGE2_FMT GX_TF_RGB565

#define SHADOW_TEX_W 256
#define SHADOW_TEX_H 256
#define SHADOW_TEX_FMT GX_TF_I8

static void
cube_lighting (void *dummy)
{
  GXLightObj lo0/*, lo1*/;
  guVector ldir;

#include "bloom-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 40, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 160, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT0/* | GX_LIGHT1*/, GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT0/* | GX_LIGHT1*/, GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  /* Another dimmer, redder light.  */
  /*guVecSub (&light1.tpos, &light1.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo1, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo1, 32);
  GX_InitLightColor (&lo1, (GXColor) { 160, 64, 64, 192 });
  GX_LoadLightObj (&lo1, GX_LIGHT1);*/

  /* Shadow colour.  */
  GX_SetTevColor (GX_TEVREG1, (GXColor) { 24, 16, 8, 0 });
}

static void
load_texmtx_for_blur (int horizontal)
{
  int i;

  for (i = 0; i < 8; i++)
    {
      Mtx texmtx;

      guMtxIdentity (texmtx);
      if (horizontal)
	guMtxTransApply (texmtx, texmtx, (((float) i) - 3.5) / 80.0,
			 0, 0);
      else
	guMtxTransApply (texmtx, texmtx, 0, (((float) i) - 3.5) / 60.0,
			 0);
      GX_LoadTexMtxImm (texmtx, GX_TEXMTX0 + i * 3, GX_MTX2x4);
    }
}

static void
bloom_composite_shader (void *dummy)
{
  #include "bloom-composite.inc"
}

static void
bloom_gaussian_shader (void *dummy)
{
  #include "bloom-gaussian.inc"
  GX_SetTevKColor (0, (GXColor) { 36, 73, 146, 255 });
  GX_SetTevKColor (1, (GXColor) { 255, 146, 73, 36 });
}

static void
bloom_gaussian2_shader (void *dummy)
{
  #include "bloom-gaussian2.inc"
  GX_SetTevKColor (0, (GXColor) { 36, 73, 146, 255 });
  GX_SetTevKColor (1, (GXColor) { 255, 146, 73, 36 });
}

static void
bloom_init_effect (void *params)
{
  bloom_data *bdata = (bloom_data *) params;
  
  guPerspective (proj, 60, 1.33f, 10.0f, 500.0f);
  
  bdata->phase = 0.0;
  bdata->phase2 = 0.0;
  
  bdata->stage1_texture = memalign (32, GX_GetTexBufferSize (STAGE1_W, STAGE1_H,
			    STAGE1_FMT, GX_FALSE, 0));
  bdata->stage2_texture = memalign (32, GX_GetTexBufferSize (STAGE2_W, STAGE2_H,
			    STAGE2_FMT, GX_FALSE, 0));
  bdata->shadow_buf = memalign (32, GX_GetTexBufferSize (SHADOW_TEX_W,
			SHADOW_TEX_H, SHADOW_TEX_FMT, GX_FALSE, 0));
  bdata->shadow_inf = create_shadow_info (8, &light0);

  GX_InitTexObj (&bdata->stage1_tex_obj, bdata->stage1_texture, STAGE1_W,
		 STAGE1_H, STAGE1_FMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_InitTexObj (&bdata->stage2_tex_obj, bdata->stage2_texture, STAGE2_W,
		 STAGE2_H, STAGE2_FMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_InitTexObj (&bdata->shadowbuf_tex_obj, bdata->shadow_buf, SHADOW_TEX_W,
		 SHADOW_TEX_H, SHADOW_TEX_FMT, GX_NEAR, GX_NEAR, GX_FALSE);

  /* First-pass object shader.  */
  bdata->cube_lighting_shader = create_shader (&cube_lighting, NULL);
  shader_append_texmap (bdata->cube_lighting_shader,
			get_utility_texture (bdata->shadow_inf->ramp_type),
			GX_TEXMAP0);
  shader_append_texmap (bdata->cube_lighting_shader,
			&bdata->shadowbuf_tex_obj, GX_TEXMAP1);
  shader_append_texcoordgen (bdata->cube_lighting_shader, GX_TEXCOORD0,
			     GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);
  shader_append_texcoordgen (bdata->cube_lighting_shader, GX_TEXCOORD1,
			     GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX1);

  /* Horizontal gaussian blur shader.  */
  bdata->gaussian_blur_shader = create_shader (&bloom_gaussian_shader, NULL);
  shader_append_texmap (bdata->gaussian_blur_shader,
			&bdata->stage1_tex_obj, GX_TEXMAP0);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX0);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX1);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX2);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX3);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX4);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX5);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD6, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX6);
  shader_append_texcoordgen (bdata->gaussian_blur_shader,
			     GX_TEXCOORD7, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX7);
  
  /* Vertical gaussian blur shader.  */
  bdata->gaussian_blur2_shader = create_shader (&bloom_gaussian2_shader, NULL);
  shader_append_texmap (bdata->gaussian_blur2_shader,
			&bdata->stage2_tex_obj, GX_TEXMAP0);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX0);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX1);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX2);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX3);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX4);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX5);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD6, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX6);
  shader_append_texcoordgen (bdata->gaussian_blur2_shader,
			     GX_TEXCOORD7, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_TEXMTX7);

  bdata->composite_shader = create_shader (&bloom_composite_shader, NULL);
  shader_append_texmap (bdata->composite_shader, &bdata->stage1_tex_obj,
			GX_TEXMAP1);
  shader_append_texcoordgen (bdata->composite_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
}

static void
bloom_uninit_effect (void *params)
{
  bloom_data *bdata = (bloom_data *) params;

  free (bdata->stage1_texture);
  free (bdata->stage2_texture);
  free (bdata->shadow_buf);
  free_shader (bdata->cube_lighting_shader);
  free_shader (bdata->gaussian_blur_shader);
  free_shader (bdata->gaussian_blur2_shader);
  destroy_shadow_info (bdata->shadow_inf);
}

static void
bloom_display_effect (uint32_t time_offset, void *params, int iparam,
		      GXRModeObj *rmode)
{
  bloom_data *bdata = (bloom_data *) params;
  object_loc softcube_loc;
  Mtx modelview, rotmtx, rotmtx2, scale;
    
  object_loc_initialise (&softcube_loc, GX_PNMTX0);
  
  scene_update_camera (&scene);
  light_update (scene.camera, &light0);
  light_update (scene.camera, &light1);
  
  guMtxIdentity (modelview);
  guMtxRotAxisDeg (rotmtx, &((guVector) {0, 1, 0}), bdata->phase);
  guMtxRotAxisDeg (rotmtx2, &((guVector) {1, 0, 0}), bdata->phase2);
  
  guMtxConcat (rotmtx, modelview, modelview);
  guMtxConcat (rotmtx2, modelview, modelview);
  guMtxScale (scale, 15.0, 15.0, 15.0);
  
  /* Render shadow buffer.  */
  {
    object_loc shadowcast_loc;

    rendertarget_texture (SHADOW_TEX_W, SHADOW_TEX_H, GX_TF_Z8);
    GX_SetPixelFmt (GX_PF_Z24, GX_ZC_LINEAR);

    GX_SetCullMode (GX_CULL_FRONT);
    
    shadow_set_bounding_radius (bdata->shadow_inf, 30.0);
    shadow_setup_ortho (bdata->shadow_inf, 5.0f, 100.0f);
    
    shadow_casting_tev_setup ();
    
    object_loc_initialise (&shadowcast_loc, GX_PNMTX0);
    
    scene_update_matrices (&scene, &shadowcast_loc,
			   bdata->shadow_inf->light_cam, modelview, scale,
			   bdata->shadow_inf->shadow_projection,
			   bdata->shadow_inf->projection_type);

    GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
    GX_SetColorUpdate (GX_FALSE);
    GX_SetAlphaUpdate (GX_FALSE);
    
    object_set_arrays (&knot_obj, OBJECT_POS, GX_VTXFMT0, 0);
    object_render (&knot_obj, OBJECT_POS, GX_VTXFMT0);
    
    GX_CopyTex (bdata->shadow_buf, GX_TRUE);
    GX_PixModeSync ();
  }
  
  /* Render knot, with shadowing from one light source and over-bright specular
     highlights in the alpha channel.  */
  
  rendertarget_texture (STAGE1_W, STAGE1_H, STAGE1_FMT);
  GX_SetPixelFmt (GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
  
  /* Use TEXMTX0 for ramp texture lookup, and TEXMTX1 for shadow-buffer
     lookup.  Object loc also stores a pointer to the shadow being cast onto
     the object.  */
  object_set_shadow_tex_matrix (&softcube_loc, GX_TEXMTX1, GX_TEXMTX0,
				bdata->shadow_inf);
  
  scene_update_matrices (&scene, &softcube_loc, scene.camera, modelview, scale,
			 proj, GX_PERSPECTIVE);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_BACK);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);
  
  shader_load (bdata->cube_lighting_shader);
  
  object_set_arrays (&knot_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0, 0);
  object_render (&knot_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);
  
  GX_CopyTex (bdata->stage1_texture, GX_TRUE);
  GX_PixModeSync ();

  rendertarget_texture (STAGE2_W, STAGE2_H, STAGE2_FMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_NONE);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  /* Draw highlights with horizontal blurring.  */
  load_texmtx_for_blur (1);
  shader_load (bdata->gaussian_blur_shader);
  screenspace_rect (bdata->gaussian_blur_shader, GX_VTXFMT1, 0);

  /* Copy to stage2_texture.  */
  GX_CopyTex (bdata->stage2_texture, GX_TRUE);
  GX_PixModeSync ();
  
  rendertarget_screen (rmode);
  
  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_NONE);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  load_texmtx_for_blur (0);
  screenspace_rect (bdata->gaussian_blur2_shader, GX_VTXFMT1, 0);

  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);

  screenspace_rect (bdata->composite_shader, GX_VTXFMT1, 0);

  bdata->phase += 1;
  bdata->phase2 += 3.0 * sinf (bdata->phase * M_PI / 180.0);
}

effect_methods bloom_methods =
{
  .preinit_assets = NULL,
  .init_effect = &bloom_init_effect,
  .prepare_frame = NULL,
  .display_effect = &bloom_display_effect,
  .uninit_effect = &bloom_uninit_effect,
  .finalize = NULL
};
