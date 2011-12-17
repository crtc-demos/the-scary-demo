#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "glass.h"
#include "ghost-obj.h"
#include "shader.h"
#include "screenspace.h"

/*
#include "images/primary.h"
#include "primary_tpl.h"

static TPLFile primaryTPL;
*/

#include "images/mighty_zebu.h"
#include "mighty_zebu_tpl.h"

static TPLFile mighty_zebuTPL;

#include "images/spiderweb.h"
#include "spiderweb_tpl.h"

static TPLFile spiderwebTPL;

#include "object.h"
#include "scene.h"
#include "rendertarget.h"

#define COPYFMT GX_TF_RGBA8
#define USEFMT GX_TF_RGBA8

#define RTT_WIDTH 640
#define RTT_HEIGHT 480

static Mtx44 perspmat;

static scene_info scene =
{
  .pos = { 0, 0, 30 },
  .up = { 0, 1, 0 },
  .lookat = { 0, 0, 0 },
  .camera_dirty = 1
};

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

glass_data glass_data_0;

static void
plain_texture_setup (void *dummy)
{
  #include "plain-texturing.inc"
}

static void
refraction_setup (void *dummy)
{
  #include "refraction.inc"
}

static void
refraction_postpass_setup (void *dummy)
{
  #include "do-refraction.inc"
}

static void
glass_postpass (void *dummy)
{
  GXLightObj lo0;
  guVector ldir;

  #include "glass-postpass.inc"

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 128);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);
}


static void
ghost_init_effect (void *params, backbuffer_info *bbuf)
{
  glass_data *gdata = (glass_data *) params;

  guPerspective (perspmat, 60, 1.33f, 10.0f, 500.0f);
  scene_update_camera (&scene);
  
  object_loc_initialise (&gdata->obj_loc, GX_PNMTX0);
  object_set_tex_norm_matrix (&gdata->obj_loc, GX_TEXMTX0);

  TPL_OpenTPLFromMemory (&mighty_zebuTPL, (void *) mighty_zebu_tpl,
			 mighty_zebu_tpl_size);
  
  TPL_OpenTPLFromMemory (&spiderwebTPL, (void *) spiderweb_tpl,
			 spiderweb_tpl_size);
  
  gdata->grabbed_texture = memalign (32, GX_GetTexBufferSize (RTT_WIDTH,
				     RTT_HEIGHT, USEFMT, GX_FALSE, 0));
  
  /* Set up refraction shader.  */
  gdata->refraction_shader = create_shader (&refraction_setup, NULL);
  shader_append_texmap (gdata->refraction_shader,
			get_utility_texture (UTIL_TEX_REFRACT),
			GX_TEXMAP2);
  shader_append_texcoordgen (gdata->refraction_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX0);

  /* Set up refraction post-pass shader.  */
  TPL_GetTexture (&mighty_zebuTPL, mighty_zebu, &gdata->mighty_zebu_tex_obj);

  GX_InitTexObjWrapMode (&gdata->mighty_zebu_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&gdata->mighty_zebu_tex_obj, GX_LINEAR, GX_LINEAR);
  
  gdata->plain_texture_shader = create_shader (&plain_texture_setup, NULL);
  shader_append_texmap (gdata->plain_texture_shader,
			&gdata->mighty_zebu_tex_obj, GX_TEXMAP0);
  shader_append_texcoordgen (gdata->plain_texture_shader,
			     GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_IDENTITY);

  GX_InitTexObj (&gdata->grabbed_tex_obj, gdata->grabbed_texture, RTT_WIDTH,
		 RTT_HEIGHT, USEFMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  gdata->refraction_postpass_shader = create_shader (&refraction_postpass_setup,
						     NULL);
  shader_append_texmap (gdata->refraction_postpass_shader,
			&gdata->grabbed_tex_obj, GX_TEXMAP0);
  shader_append_texmap (gdata->refraction_postpass_shader,
			&gdata->mighty_zebu_tex_obj, GX_TEXMAP1);
  shader_append_texcoordgen (gdata->refraction_postpass_shader,
			     GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0,
			     GX_IDENTITY);
  shader_append_texcoordgen (gdata->refraction_postpass_shader,
			     GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1,
			     GX_IDENTITY);

  /* Set up glass post-pass shader.  */
  gdata->glass_postpass_shader = create_shader (&glass_postpass, NULL);
  shader_append_texmap (gdata->glass_postpass_shader,
			get_utility_texture (UTIL_TEX_DARKENING), GX_TEXMAP3);
  shader_append_texcoordgen (gdata->glass_postpass_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX0);
}

static void
ghost_uninit_effect (void *params, backbuffer_info *bbuf)
{
  glass_data *gdata = (glass_data *) params;

  free_shader (gdata->refraction_shader);
  free_shader (gdata->refraction_postpass_shader);
  free (gdata->grabbed_texture);
}

static display_target
ghost_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  glass_data *gdata = (glass_data *) params;
  /*GXTexObj spiderweb_tex_obj;*/
  Mtx mvtmp, rot, mvtmp2;
  Mtx sep_scale;

  /*TPL_GetTexture (&spiderwebTPL, spiderweb, &spiderweb_tex_obj);*/

  GX_InvalidateTexAll ();
  
  rendertarget_texture (RTT_WIDTH, RTT_HEIGHT, COPYFMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  screenspace_rect (gdata->plain_texture_shader, GX_VTXFMT1, 0);
  
  GX_SetCopyClear ((GXColor) { 128, 128, 128, 0 }, 0x00ffffff);
  
  GX_CopyTex (gdata->grabbed_texture, GX_TRUE);
  
  GX_PixModeSync ();

  rendertarget_texture (RTT_WIDTH, RTT_HEIGHT, COPYFMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_SetCopyClear ((GXColor) { 128, 128, 128, 0 }, 0x00ffffff);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  /* We need a grey background!  This isn't very efficient though.  */
  GX_CopyTex (gdata->grabbed_texture, GX_TRUE);

  GX_SetCopyClear ((GXColor) { 0, 0, 0, 0 }, 0x00ffffff);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  GX_SetCullMode (GX_CULL_NONE);

  guMtxIdentity (mvtmp);
  guMtxRotAxisDeg (rot, &((guVector) { 0, 1, 0 }), gdata->thr);
  guMtxConcat (rot, mvtmp, mvtmp);
  guMtxRotAxisDeg (rot, &((guVector) { 1, 0, 0 }), gdata->thr * 0.7);
  guMtxConcat (rot, mvtmp, mvtmp);
  
  guMtxScale (sep_scale, 6.0, 6.0, 6.0);
  scene_update_matrices (&scene, &gdata->obj_loc, scene.camera, mvtmp,
			 sep_scale, perspmat, GX_PERSPECTIVE);

  light_update (scene.camera, &light0);
  
  shader_load (gdata->refraction_shader);
  
  object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, 13, 0, 0);
  scene_update_matrices (&scene, &gdata->obj_loc, scene.camera, mvtmp2,
			 sep_scale, NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, -13, 0, 0);
  scene_update_matrices (&scene, &gdata->obj_loc, scene.camera, mvtmp2,
			 sep_scale, NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  GX_CopyTex (gdata->grabbed_texture, GX_TRUE);
  
  GX_PixModeSync ();

  return MAIN_BUFFER;
}

static void
ghost_display_effect (uint32_t time_offset, void *params, int iparam)
{
  glass_data *gdata = (glass_data *) params;
  /*GXTexObj spiderweb_tex_obj;*/
  Mtx mvtmp, rot, mvtmp2;
  Mtx sep_scale;
  f32 indmtx[2][3];

  /* Channels for indirect lookup go:
       R -> -
       G -> U
       B -> T
       A -> S
     We have G/B channels, so map those to S/T.  */

  indmtx[0][0] = 0.0; indmtx[0][1] = 0.5; indmtx[0][2] = 0.0;
  indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.5;

  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 2);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  GX_SetCullMode (GX_CULL_NONE);

  screenspace_rect (gdata->refraction_postpass_shader, GX_VTXFMT1, 1);
  
  /*GX_LoadTexObj (&spiderweb_tex_obj, GX_TEXMAP1);
  
  draw_square (1, 128 + 127 * sin (thr * M_PI / 360.0),
	       128 + 127 * sin (thr * M_PI / 200.0));*/

  guMtxIdentity (mvtmp);
  guMtxRotAxisDeg (rot, &((guVector) { 0, 1, 0 }), gdata->thr);
  guMtxConcat (rot, mvtmp, mvtmp);
  guMtxRotAxisDeg (rot, &((guVector) { 1, 0, 0 }), gdata->thr * 0.7);
  guMtxConcat (rot, mvtmp, mvtmp);

  guMtxScale (sep_scale, 6.0, 6.0, 6.0);
  scene_update_matrices (&scene, &gdata->obj_loc, scene.camera, mvtmp,
			 sep_scale, perspmat, GX_PERSPECTIVE);

  shader_load (gdata->glass_postpass_shader);

  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_SRCALPHA, GX_LO_SET);

  object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, 13, 0, 0);
  scene_update_matrices (&scene, &gdata->obj_loc, scene.camera, mvtmp2,
			 sep_scale, NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, -13, 0, 0);
  scene_update_matrices (&scene, &gdata->obj_loc, scene.camera, mvtmp2,
			 sep_scale, NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  gdata->thr += 1;
}

effect_methods glass_methods =
{
  .preinit_assets = NULL,
  .init_effect = &ghost_init_effect,
  .prepare_frame = &ghost_prepare_frame,
  .display_effect = &ghost_display_effect,
  .composite_effect = NULL,
  .uninit_effect = &ghost_uninit_effect,
  .finalize = NULL
};
