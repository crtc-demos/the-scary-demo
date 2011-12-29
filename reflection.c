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
#include "reflection.h"
#include "world.h"
#include "ghost-obj.h"

#define USE_EMBM

#include "objects/scary-skull-2.inc"

INIT_OBJECT (scary_skull_obj, scary_skull);

#include "images/skull_tangentmap_gx.h"
#include "skull_tangentmap_gx_tpl.h"

TPLFile skull_tangentmapTPL;

static Mtx44 cubeface_proj;

reflection_data reflection_data_0;

static void
plain_texturing_setup (void *dummy)
{
  #include "plain-texturing.inc"
}

#ifndef USE_EMBM
static int colourswitch;

static void
envmap_setup (void *dummy)
{
  int amount = colourswitch % 100;
  
  amount = amount < 50 ? amount * 2 : (100 - amount) * 2;
  
  if (colourswitch < 100)
    GX_SetTevColor (GX_TEVREG0, (GXColor) { 255, 128, 0, amount });
  else
    GX_SetTevColor (GX_TEVREG0, (GXColor) { 0, 192, 0, amount });

  #include "fancy-envmap.inc"
  
  colourswitch++;
  if (colourswitch > 200)
    colourswitch = 0;
}
#endif

#ifdef USE_EMBM
static void
envmap_setup_2 (void *dummy)
{
  f32 indmtx[2][3] = { { 0.5, 0.0, 0.0 },
		       { 0.0, 0.5, 0.0 } };
  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 0);
  #include "embm.inc"
}
#endif

static void
reflection_init_effect (void *params, backbuffer_info *bbuf)
{
  reflection_data *rdata = (reflection_data *) params;

  guPerspective (cubeface_proj, 90, 1.0f, 10.0f, 500.0f);

  rdata->skybox = create_skybox (200);
  rdata->cubemap = create_cubemap (256, GX_TF_RGB565, 512, GX_TF_RGB565);
  
  rdata->plain_texture_shader = create_shader (&plain_texturing_setup, NULL);
  shader_append_texmap (rdata->plain_texture_shader, &rdata->cubemap->spheretex,
			GX_TEXMAP0);
  shader_append_texcoordgen (rdata->plain_texture_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  
  rdata->world = create_world (0);
  world_set_perspective (rdata->world, 60, 1.33f, 10.0f, 500.0f);
  world_set_pos_lookat_up (rdata->world, (guVector) { 0, 0, 30},
					 (guVector) { 0, 0, 0 },
					 (guVector) { 0, 1, 0 });

  object_loc_initialise (&rdata->ghost_loc, GX_PNMTX0);
  object_set_sph_envmap_matrix (&rdata->ghost_loc, GX_TEXMTX0);

  guMtxIdentity (rdata->ghost_mv);
  guMtxScale (rdata->ghost_scale, 10, 10, 10);

#ifdef USE_EMBM
  TPL_OpenTPLFromMemory (&skull_tangentmapTPL, (void *) skull_tangentmap_gx_tpl,
			 skull_tangentmap_gx_tpl_size);
  TPL_GetTexture (&skull_tangentmapTPL, skull_tangentmap, &rdata->tangentmap);

  object_set_tex_norm_binorm_matrices (&rdata->ghost_loc, GX_TEXMTX1,
				       GX_TEXMTX2);

  rdata->ghost_shader = create_shader (&envmap_setup_2, NULL);
  shader_append_texmap (rdata->ghost_shader, &rdata->cubemap->spheretex,
			GX_TEXMAP0);
  shader_append_texmap (rdata->ghost_shader, &rdata->tangentmap, GX_TEXMAP1);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD1, GX_TG_MTX2x4,
			     GX_TG_BINRM, GX_TEXMTX2);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD2, GX_TG_MTX2x4,
			     GX_TG_TANGENT, GX_TEXMTX2);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD3, GX_TG_MTX2x4,
			     GX_TG_NRM, GX_TEXMTX1);
#else
  object_set_tex_norm_matrix (&rdata->ghost_loc, GX_TEXMTX1);

  rdata->ghost_shader = create_shader (&envmap_setup, NULL);
  shader_append_texmap (rdata->ghost_shader, &rdata->cubemap->spheretex,
			GX_TEXMAP0);
  shader_append_texmap (rdata->ghost_shader,
			get_utility_texture (UTIL_TEX_DARKENING), GX_TEXMAP1);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_NRM, GX_TEXMTX0);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD1, GX_TG_MTX2x4,
			     GX_TG_NRM, GX_TEXMTX1);

#endif

  world_add_standard_object (rdata->world, &scary_skull_obj, &rdata->ghost_loc,
			     OBJECT_POS | OBJECT_TEXCOORD | OBJECT_NBT3,
			     GX_VTXFMT0, GX_VA_TEX0, &rdata->ghost_mv,
			     &rdata->ghost_scale, rdata->ghost_shader);
}

static void
reflection_uninit_effect (void *params, backbuffer_info *bbuf)
{
  reflection_data *rdata = (reflection_data *) params;

  free_skybox (rdata->skybox);
  free_cubemap (rdata->cubemap);
  free_shader (rdata->plain_texture_shader);
  free_world (rdata->world);
}

static float around = 0.0;
static float up = 0.0;

static display_target
reflection_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  reflection_data *rdata = (reflection_data *) params;
  int i;

  around += PAD_StickX (0) / 300.0;
  up += PAD_StickY (0) / 300.0;

  scene_set_pos (&rdata->world->scene,
		 (guVector) { 30 * cosf (around) * cosf (up),
			      30 * sinf (up),
			      30 * sinf (around) * cosf (up) });

  scene_update_camera (&rdata->world->scene);

  for (i = 0; i < 6; i++)
    {
      Mtx camera;
      
      cubemap_cam_matrix_for_face (camera, &rdata->world->scene, i);
      
      rendertarget_texture (rdata->cubemap->size, rdata->cubemap->size,
			    rdata->cubemap->fmt);
      
      skybox_set_matrices (&rdata->world->scene, camera, rdata->skybox,
			   cubeface_proj, GX_PERSPECTIVE);
      skybox_render (rdata->skybox);
      
      GX_CopyTex (rdata->cubemap->texels[i], GX_TRUE);
    }
  
  GX_PixModeSync ();

  reduce_cubemap (rdata->cubemap, 32);

  return MAIN_BUFFER;
}

static void
reflection_display_effect (uint32_t time_offset, void *params, int iparam)
{
  reflection_data *rdata = (reflection_data *) params;

#if 0
  screenspace_rect (rdata->plain_texture_shader, GX_VTXFMT0, 0);
#else
  skybox_set_matrices (&rdata->world->scene, rdata->world->scene.camera,
		       rdata->skybox, rdata->world->projection,
		       rdata->world->projection_type);
  skybox_render (rdata->skybox);
  world_display (rdata->world);
#endif
}

effect_methods reflection_methods =
{
  .preinit_assets = NULL,
  .init_effect = &reflection_init_effect,
  .prepare_frame = &reflection_prepare_frame,
  .display_effect = &reflection_display_effect,
  .uninit_effect = &reflection_uninit_effect,
  .finalize = NULL
};
