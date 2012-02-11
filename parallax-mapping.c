#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "rendertarget.h"
#include "parallax-mapping.h"
#include "light.h"
#include "object.h"
#include "scene.h"
#include "server.h"
#include "lighting-texture.h"
#include "screenspace.h"
#include "matrixutil.h"

#include "objects/cobra.inc"

INIT_OBJECT (cobra_obj, cobra);

#include "objects/column.inc"

INIT_OBJECT (column_obj, column);

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
  .pos = { 20, 20, 50 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

#include "images/snakytextures.h"
#include "snakytextures_tpl.h"

static TPLFile snakytextureTPL;

#include "cam-path.h"
#include "objects/cobra9.xyz"

/* These settings are kind of hackish & arbitrary...  */
#define BUMP_DISPLACEMENT_STRENGTH -3
#define BUMP_TANGENT_STRENGTH -1

#define TEXCOORD_MAP_H 512
#define TEXCOORD_MAP_W 512
#define TEXCOORD_MAP_FMT GX_TF_IA8

#define TEXCOORD_MAP2_H 512
#define TEXCOORD_MAP2_W 512
#define TEXCOORD_MAP2_FMT GX_TF_IA8

parallax_mapping_data parallax_mapping_data_0;

static void
texturing (void *dummy)
{
  #include "parallax-lit-phase1.inc"
}

static void
parallax_phase2 (void *dummy)
{
  #include "parallax-lit-phase2.inc"
}

static void
parallax_phase3 (void *dummy)
{
  #include "parallax-lit-phase3.inc"
}

static void
column_shader_setup (void *dummy)
{
  GXLightObj lo0;
  guVector ldir;

  #include "column-texture.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 224, 224, 224, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);
}

static void
fog_shader_setup (void *dummy)
{
  #include "fog-texture.inc"
}

static void
tunnel_lighting (void *dummy)
{
  GXLightObj lo0;
  guVector ldir;

#include "tunnel-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 224, 224, 224, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  //GX_SetTevKColor (0, (GXColor) { 255, 192, 0, 0 });
}

static void
parallax_mapping_init_effect (void *params, backbuffer_info *bbuf)
{
  parallax_mapping_data *pdata = (parallax_mapping_data *) params;
  unsigned int tex_edge_length;
  guPerspective (perspmat, 45, 1.33f, 1.0f, 500.0f);

  tex_edge_length = 1024;
  
  object_loc_initialise (&pdata->obj_loc, GX_PNMTX0);
  object_set_parallax_tex_matrices (&pdata->obj_loc, GX_TEXMTX0, GX_TEXMTX1,
				    tex_edge_length);

  TPL_OpenTPLFromMemory (&snakytextureTPL, (void *) snakytextures_tpl,
			 snakytextures_tpl_size);

  pdata->lighting_texture = create_lighting_texture ();
  pdata->texcoord_map = memalign (32, GX_GetTexBufferSize (TEXCOORD_MAP_W,
			  TEXCOORD_MAP_H, TEXCOORD_MAP_FMT, GX_FALSE, 0));
  pdata->texcoord_map2 = memalign (32, GX_GetTexBufferSize (TEXCOORD_MAP2_W,
			   TEXCOORD_MAP2_H, TEXCOORD_MAP2_FMT, GX_FALSE, 0));
			   
  pdata->tunnel_lighting_shader = create_shader (&tunnel_lighting, NULL);

  GX_InitTexObj (&pdata->texcoord_map_obj, pdata->texcoord_map, TEXCOORD_MAP_W,
		 TEXCOORD_MAP_H, TEXCOORD_MAP_FMT, GX_CLAMP, GX_CLAMP,
		 GX_FALSE);
  GX_InitTexObjFilterMode (&pdata->texcoord_map_obj, GX_LINEAR, GX_LINEAR);

  GX_InitTexObj (&pdata->texcoord_map2_obj, pdata->texcoord_map2,
		 TEXCOORD_MAP2_W, TEXCOORD_MAP2_H, TEXCOORD_MAP2_FMT, GX_CLAMP,
		 GX_CLAMP, GX_FALSE);
  GX_InitTexObjFilterMode (&pdata->texcoord_map2_obj, GX_LINEAR, GX_LINEAR);

  /* Phase 1.  */
  TPL_GetTexture (&snakytextureTPL, snake_offset, &pdata->stone_depth_obj);
  GX_InitTexObjWrapMode (&pdata->stone_depth_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&pdata->stone_depth_obj, GX_LINEAR, GX_LINEAR);
  pdata->parallax_lit_phase1_shader = create_shader (&texturing, NULL);
  shader_append_texmap (pdata->parallax_lit_phase1_shader,
			&pdata->stone_depth_obj, GX_TEXMAP1);
  shader_append_texmap (pdata->parallax_lit_phase1_shader,
			get_utility_texture (UTIL_TEX_16BIT_RAMP_NOREPEAT),
			GX_TEXMAP4);
  shader_append_texcoordgen (pdata->parallax_lit_phase1_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  shader_append_texcoordgen (pdata->parallax_lit_phase1_shader, GX_TEXCOORD1,
			     GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX0);
  shader_append_texcoordgen (pdata->parallax_lit_phase1_shader, GX_TEXCOORD2,
			     GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX1);

  /* Phase 2.  */
  TPL_GetTexture (&snakytextureTPL, snake_tanmap, &pdata->height_bump_obj);
  pdata->parallax_lit_phase2_shader = create_shader (&parallax_phase2, NULL);
  shader_append_texmap (pdata->parallax_lit_phase2_shader,
			&pdata->height_bump_obj, GX_TEXMAP3);
  shader_append_texmap (pdata->parallax_lit_phase2_shader,
			&pdata->texcoord_map_obj, GX_TEXMAP5);
  shader_append_texcoordgen (pdata->parallax_lit_phase2_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

  /* Phase 3.  */
  TPL_GetTexture (&snakytextureTPL, snake_texture, &pdata->stone_tex_obj);
  //GX_InitTexObjMaxAniso (&stone_tex_obj, GX_ANISO_4);
  
  GX_InitTexObjWrapMode (&pdata->stone_tex_obj, GX_CLAMP, GX_CLAMP);

  pdata->parallax_lit_phase3_shader = create_shader (&parallax_phase3, NULL);
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			&pdata->stone_tex_obj, GX_TEXMAP0);
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			&pdata->stone_depth_obj, GX_TEXMAP1);
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			&pdata->lighting_texture->texobj, GX_TEXMAP2);
  /* Temporary... */
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			&pdata->height_bump_obj, GX_TEXMAP3);
  /* End temp.  */
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			get_utility_texture (UTIL_TEX_16BIT_RAMP_NOREPEAT),
			GX_TEXMAP4);
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			&pdata->texcoord_map_obj, GX_TEXMAP5);
  shader_append_texmap (pdata->parallax_lit_phase3_shader,
			&pdata->texcoord_map2_obj, GX_TEXMAP6);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD1,
			     GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX0);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD2,
			     GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX1);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD3,
			     GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX3);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD4,
			     GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX3);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD5,
			     GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX2);
  shader_append_texcoordgen (pdata->parallax_lit_phase3_shader, GX_TEXCOORD6,
			     GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX4);

  /* Column bits.  */
  TPL_GetTexture (&snakytextureTPL, column_texture,
		  &pdata->column_texture_obj);

  pdata->column_shader = create_shader (&column_shader_setup, NULL);
  shader_append_texmap (pdata->column_shader, &pdata->column_texture_obj,
			GX_TEXMAP0);
  shader_append_texcoordgen (pdata->column_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);

  /* Fog bits.  */
  TPL_GetTexture (&snakytextureTPL, fog_texture, &pdata->fog_texture_obj);
  
  pdata->fog_shader = create_shader (&fog_shader_setup, NULL);
  shader_append_texmap (pdata->fog_shader, &pdata->fog_texture_obj,
			GX_TEXMAP0);
  shader_append_texcoordgen (pdata->fog_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);
}

static void
parallax_mapping_uninit_effect (void *params, backbuffer_info *bbuf)
{
  parallax_mapping_data *pdata = (parallax_mapping_data *) params;

  free_lighting_texture (pdata->lighting_texture);
  free (pdata->texcoord_map);
  free (pdata->texcoord_map2);
  free_shader (pdata->tunnel_lighting_shader);
  free_shader (pdata->parallax_lit_phase1_shader);
  free_shader (pdata->parallax_lit_phase2_shader);
  free_shader (pdata->parallax_lit_phase3_shader);
  free_shader (pdata->column_shader);
  free_shader (pdata->fog_shader);
}

static float around = 0.0;
static float up = 0.0;

static float lightrot = 0.0;

static display_target
parallax_mapping_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  parallax_mapping_data *pdata = (parallax_mapping_data *) params;
  object_info *render_object;
  Mtx /*rot,*/ id;

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetCullMode (GX_CULL_BACK);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  GX_SetFog (GX_FOG_NONE, 0, 1, 0, 1, (GXColor) { 0, 0, 0, 0 });

  render_object = &cobra_obj;

  around += PAD_StickX (0) / 300.0;
  up += PAD_StickY (0) / 300.0;

  light0.pos.x = 40 * cosf (lightrot);
  light0.pos.y = 40;
  light0.pos.z = 40 * sinf (lightrot);
  
  lightrot += 0.2;

  /*scene_set_pos (&scene,
		 (guVector) { 50 * cosf (around) * cosf (up),
			      50 * sinf (up),
			      50 * sinf (around) * cosf (up) });*/
  guMtxScale (id, 10, 10, 10);
  matrixutil_swap_yz (id, id);
  cam_path_follow (&scene, id, &cobra9, (float) time_offset / 10000.0);

  scene_update_camera (&scene);

  /* PHASE 0: Set up specular lighting texture.  */

  shader_load (pdata->tunnel_lighting_shader);
  light_update (scene.camera, &light0);
  update_lighting_texture (&scene, pdata->lighting_texture);

  /* PHASE 1: Render bump offset to screen-space indirect texture.  */

  rendertarget_texture (TEXCOORD_MAP_W, TEXCOORD_MAP_H, GX_CTF_GB8);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
      
  /*pdata->phase += (float) PAD_StickX (0) / 100.0;
  pdata->phase2 += (float) PAD_StickY (0) / 100.0;*/
  
  guMtxIdentity (pdata->modelview);
  /*guMtxRotAxisDeg (rot, &((guVector) { 0, 1, 0 }), pdata->phase);
  guMtxConcat (rot, pdata->modelview, pdata->modelview);
  guMtxRotAxisDeg (rot, &((guVector) { 1, 0, 0 }), pdata->phase2);
  guMtxConcat (rot, pdata->modelview, pdata->modelview);*/
  //guMtxTransApply (pdata->modelview, pdata->modelview, 0, -20, 0);

  guMtxScale (pdata->scale, 10.0, 10.0, 10.0);
  
  object_set_matrices (&scene, &pdata->obj_loc, scene.camera,
		       pdata->modelview, pdata->scale, perspmat,
		       GX_PERSPECTIVE);
  
  object_set_arrays (render_object,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  GX_SetCurrentMtx (GX_PNMTX0);
  
  shader_load (pdata->parallax_lit_phase1_shader);

  {
    f32 indmtx[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    /* Bump-height offset strength.  */
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, BUMP_DISPLACEMENT_STRENGTH);
  }
  
  GX_SetCullMode (GX_CULL_BACK);
  object_render (render_object, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  GX_CopyTex (pdata->texcoord_map, GX_TRUE);
  GX_PixModeSync ();

  /* PHASE 2: Create new texture containing bump s,t offset texture lookup
     results -- in screen space.  */

  {
    f32 indmtx[2][3] = { { 0, 0.5, 0 }, { 0.5, 0, 0 } };
    /* This wants to be: log2(size of bump s,t-offset texture) - 7.  */
    GX_SetIndTexMatrix (GX_ITM_2, indmtx, 1);
  }

  rendertarget_texture (TEXCOORD_MAP2_W, TEXCOORD_MAP2_H, GX_CTF_GB8);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  screenspace_rect (pdata->parallax_lit_phase2_shader, GX_VTXFMT1, 0);

  GX_CopyTex (pdata->texcoord_map2, GX_TRUE);
  GX_PixModeSync ();
  
  return MAIN_BUFFER;
}

static void
parallax_mapping_display_effect (uint32_t time_offset, void *params, int iparam)
{
  parallax_mapping_data *pdata = (parallax_mapping_data *) params;
  object_loc map_flat_loc;
  object_info *render_object;
  Mtx col_mv, col_scale;
  int i;

  render_object = &cobra_obj;
  
  /* PHASE 3: Put things on the screen.  */

  {
    f32 indmtx[2][3] = { { 0.5, 0, 0 }, { 0, 0.5, 0 } };
    /* Bump-height offset strength.  */
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, BUMP_DISPLACEMENT_STRENGTH);
    /* Strength of binorm/tangent offset.  */
    GX_SetIndTexMatrix (GX_ITM_1, indmtx, BUMP_TANGENT_STRENGTH);
  }

  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  /* We want alpha blending.  */
  //GX_SetBlendMode (GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  
  object_loc_initialise (&map_flat_loc, GX_PNMTX0);
  object_set_screenspace_tex_matrix (&map_flat_loc, GX_TEXMTX4);
  object_set_tex_norm_binorm_matrices (&map_flat_loc, GX_TEXMTX2, GX_TEXMTX3);

  shader_load (pdata->parallax_lit_phase3_shader);

  object_set_matrices (&scene, &map_flat_loc, scene.camera, pdata->modelview,
		       pdata->scale, perspmat, GX_PERSPECTIVE);
  object_set_arrays (render_object, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);
  object_render (render_object, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  /* Render the nice columns.  */

  shader_load (pdata->column_shader);
  
  guMtxScale (col_scale, 10.0, 10.0, 10.0);
  object_set_arrays (&column_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  for (i = 0; i < 6; i++)
    {
      guMtxTrans (col_mv, 75.0, 0.0, 25.0 + i * 75);

      object_set_matrices (&scene, &pdata->column_loc, scene.camera, col_mv,
			   col_scale, NULL, 0);
      object_render (&column_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		     GX_VTXFMT0);

      guMtxTrans (col_mv, -75.0, 0.0, 25.0 + i * 75);
      object_set_matrices (&scene, &pdata->column_loc, scene.camera, col_mv,
			   col_scale, NULL, 0);
      object_render (&column_obj, OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD,
		     GX_VTXFMT0);
    }
  
  /*screenspace_rect (pdata->fog_shader, GX_VTXFMT0, 0);*/
}

effect_methods parallax_mapping_methods =
{
  .preinit_assets = NULL,
  .init_effect = &parallax_mapping_init_effect,
  .prepare_frame = &parallax_mapping_prepare_frame,
  .display_effect = &parallax_mapping_display_effect,
  .uninit_effect = &parallax_mapping_uninit_effect,
  .finalize = NULL
};
