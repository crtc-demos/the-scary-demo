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

#define USE_DISPLAY_LISTS
#define RIB_DL_SIZE 32768

#include "objects/scary-skull-2.inc"

INIT_OBJECT (scary_skull_obj, scary_skull);

#include "objects/rib.inc"

INIT_OBJECT (rib_obj, rib);

#include "objects/rib-lo.inc"

INIT_OBJECT (rib_lo_obj, rib_lo);

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
rib_shader_setup (void *privdata)
{
  light_info *light = (light_info *) privdata;
  GXLightObj lo0;

  #include "plain-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 224, 224, 224, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_InitLightPos (&lo0, light->tpos.x, light->tpos.y, light->tpos.z);
  GX_InitLightColor (&lo0, (GXColor) { 255, 255, 255, 255 });
  GX_InitLightSpot (&lo0, 0.0, GX_SP_OFF);
  GX_InitLightDistAttn (&lo0, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightDir (&lo0, 0.0, 0.0, 0.0);
  GX_LoadLightObj (&lo0, GX_LIGHT0);
}

static void
reflection_init_effect (void *params, backbuffer_info *bbuf)
{
  reflection_data *rdata = (reflection_data *) params;

  guPerspective (cubeface_proj, 90, 1.0f, 0.2f, 800.0f);

  rdata->skybox = create_skybox (800.0 / sqrtf (3.0));
  rdata->cubemap = create_cubemap (256, GX_TF_RGB565, 512, GX_TF_RGB565);
  
  rdata->plain_texture_shader = create_shader (&plain_texturing_setup, NULL);
  shader_append_texmap (rdata->plain_texture_shader, &rdata->cubemap->spheretex,
			GX_TEXMAP0);
  shader_append_texcoordgen (rdata->plain_texture_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  
  rdata->world = create_world (1);
  world_set_perspective (rdata->world, 60, 1.33f, 10.0f, 800.0f);
  world_set_pos_lookat_up (rdata->world, (guVector) { 0, 0, 30},
					 (guVector) { 0, 0, 0 },
					 (guVector) { 0, 1, 0 });
  world_set_light_pos_lookat_up (rdata->world, 0,
				 (guVector) { 20, 20, 30 },
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
  TPL_GetTexture (&skull_tangentmapTPL, skull_aomap, &rdata->aomap);

  object_set_tex_norm_binorm_matrices (&rdata->ghost_loc, GX_TEXMTX1,
				       GX_TEXMTX2);

  rdata->ghost_shader = create_shader (&envmap_setup_2, NULL);
  shader_append_texmap (rdata->ghost_shader, &rdata->cubemap->spheretex,
			GX_TEXMAP0);
  shader_append_texmap (rdata->ghost_shader, &rdata->tangentmap, GX_TEXMAP1);
  shader_append_texmap (rdata->ghost_shader, &rdata->aomap, GX_TEXMAP2);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD1, GX_TG_MTX2x4,
			     GX_TG_BINRM, GX_TEXMTX2);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD2, GX_TG_MTX2x4,
			     GX_TG_TANGENT, GX_TEXMTX2);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD3, GX_TG_MTX2x4,
			     GX_TG_NRM, GX_TEXMTX1);
  shader_append_texcoordgen (rdata->ghost_shader, GX_TEXCOORD4, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_TEXMTX3);

  {
    Mtx texmtx;
    
    guMtxScale (texmtx, 1.0, -1.0, 1.0);
    guMtxTransApply (texmtx, texmtx, 0.0, 1.0, 0.0);
    
    GX_LoadTexMtxImm (texmtx, GX_TEXMTX3, GX_MTX2x4);
  }
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

  object_loc_initialise (&rdata->rib_loc, GX_PNMTX0);

  guMtxIdentity (rdata->rib_mv);
  guMtxScale (rdata->rib_scale, 5, 5, 5);
  rdata->rib_shader = create_shader (&rib_shader_setup,
				     &rdata->world->light[0]);

  /*world_add_standard_object (rdata->world, &rib_obj, &rdata->rib_loc,
			     OBJECT_POS | OBJECT_NORM, GX_VTXFMT0, 0,
			     &rdata->rib_mv, &rdata->rib_scale,
			     rdata->rib_shader);*/

#ifdef USE_DISPLAY_LISTS
  rdata->rib_dl = memalign (32, RIB_DL_SIZE);
  rdata->rib_dl_size = 0;
  rdata->rib_lo_dl = memalign (32, RIB_DL_SIZE);
  rdata->rib_lo_dl_size = 0;
#endif
}

static void
reflection_uninit_effect (void *params, backbuffer_info *bbuf)
{
  reflection_data *rdata = (reflection_data *) params;

  free_skybox (rdata->skybox);
  free_cubemap (rdata->cubemap);
  free_shader (rdata->plain_texture_shader);
  free_shader (rdata->ghost_shader);
  free_shader (rdata->rib_shader);
  free_world (rdata->world);
#ifdef USE_DISPLAY_LISTS
  free (rdata->rib_dl);
  free (rdata->rib_lo_dl);
#endif
}

static float rib_offset = 0;

static void
rib_render (reflection_data *rdata, Mtx camera, int lo, float twist)
{
  int i;
  object_info *which_obj;
  void *disp_list;
  u32 *dl_size;
  int num_ribs = lo ? 10 : 30;
  float midpt = (num_ribs - 1) * 0.5;

  shader_load (rdata->rib_shader);
  
  if (lo)
    {
      which_obj = &rib_lo_obj;
      disp_list = rdata->rib_lo_dl;
      dl_size = &rdata->rib_lo_dl_size;
    }
  else
    {
      which_obj = &rib_obj;
      disp_list = rdata->rib_dl;
      dl_size = &rdata->rib_dl_size;
    }

  object_set_arrays (which_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0, 0);

#ifdef USE_DISPLAY_LISTS
  if (*dl_size == 0)
    {
      GX_BeginDispList (disp_list, RIB_DL_SIZE);
      object_render (which_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);
      *dl_size = GX_EndDispList ();
      srv_printf ("Initialised display list for %s rib object (%u bytes)\n",
		  lo ? "lo" : "hi", *dl_size);
      DCInvalidateRange (disp_list, *dl_size);
    }

  if (*dl_size == 0)
    {
      srv_printf ("display list didn't fit\n");
      return;
    }
#endif

  for (i = 0; i < num_ribs; i++)
    {
      if (lo || i < 10 || i >= 20)
	{
	  which_obj = &rib_lo_obj;
	  disp_list = rdata->rib_lo_dl;
	  dl_size = &rdata->rib_lo_dl_size;
	}
      else
	{
	  which_obj = &rib_obj;
	  disp_list = rdata->rib_dl;
	  dl_size = &rdata->rib_dl_size;
	}

      object_set_arrays (which_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0, 0);

      guMtxIdentity (rdata->rib_mv);
      guMtxRotRad (rdata->rib_mv, 'z', ((i - midpt) + rib_offset / 20.0)
				       * twist);
      guMtxTransApply (rdata->rib_mv, rdata->rib_mv, 0, 0,
		       (i - midpt) * 20 + rib_offset);
    
      object_set_matrices (&rdata->world->scene, &rdata->rib_loc,
			   camera, rdata->rib_mv, rdata->rib_scale,
			   NULL, 0);

#ifdef USE_DISPLAY_LISTS
      if (*dl_size)
        GX_CallDispList (disp_list, *dl_size);
      else
#endif
        object_render (which_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);
    }
}

static float around = 0.0;
static float up = 0.0;
static float twisty = 0.0;

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

  /* We only need to set this once...  */
  GX_LoadProjectionMtx (cubeface_proj, GX_PERSPECTIVE);

  for (i = 0; i < 6; i++)
    {
      Mtx camera;
      
      cubemap_cam_matrix_for_face (camera, &rdata->world->scene, i);
      
      rendertarget_texture (rdata->cubemap->size, rdata->cubemap->size,
			    rdata->cubemap->fmt);
      
      rib_render (rdata, camera, 1, 0.3 * sinf (twisty));

      skybox_set_matrices (&rdata->world->scene, camera, rdata->skybox,
			   NULL, 0);
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
  world_display (rdata->world);
  
  rib_render (rdata, rdata->world->scene.camera, 0, 0.3 * sinf (twisty));

  skybox_set_matrices (&rdata->world->scene, rdata->world->scene.camera,
		       rdata->skybox, rdata->world->projection,
		       rdata->world->projection_type);
  skybox_render (rdata->skybox);

  rib_offset += 0.5;
  if (rib_offset >= 20)
    rib_offset = 0;
  
  twisty += 0.05;
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
