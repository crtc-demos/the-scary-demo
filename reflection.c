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

static Mtx44 perspmat;
static Mtx44 cubeface_proj;

static scene_info scene =
{
  .pos = { 0, 0, 30 },
  .up = { 0, 1, 0 },
  .lookat = { 0, 0, 0 },
  .camera_dirty = 1
};

reflection_data reflection_data_0;

static void
plain_texturing_setup (void *dummy)
{
  #include "plain-texturing.inc"
}

static void
reflection_init_effect (void *params, backbuffer_info *bbuf)
{
  reflection_data *rdata = (reflection_data *) params;
  guPerspective (perspmat, 60, 1.33f, 10.0f, 500.0f);

  guPerspective (cubeface_proj, 90, 1.0f, 10.0f, 500.0f);

  rdata->skybox = create_skybox (200);
  rdata->cubemap = create_cubemap (256, GX_TF_RGB565, 512, GX_TF_RGB565);
  
  rdata->plain_texture_shader = create_shader (&plain_texturing_setup, NULL);
  shader_append_texmap (rdata->plain_texture_shader, &rdata->cubemap->spheretex,
			GX_TEXMAP0);
  shader_append_texcoordgen (rdata->plain_texture_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
}

static void
reflection_uninit_effect (void *params, backbuffer_info *bbuf)
{
  reflection_data *rdata = (reflection_data *) params;

  free_skybox (rdata->skybox);
  free_cubemap (rdata->cubemap);
  free_shader (rdata->plain_texture_shader);
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

  scene_set_pos (&scene,
		 (guVector) { 30 * cosf (around) * cosf (up),
			      30 * sinf (up),
			      30 * sinf (around) * cosf (up) });

  scene_update_camera (&scene);

  for (i = 0; i < 6; i++)
    {
      Mtx camera;
      
      cubemap_cam_matrix_for_face (camera, &scene, i);
      
      rendertarget_texture (rdata->cubemap->size, rdata->cubemap->size,
			    rdata->cubemap->fmt);
      
      skybox_set_matrices (&scene, camera, rdata->skybox, cubeface_proj,
			   GX_PERSPECTIVE);
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
  
  /*skybox_set_matrices (&scene, rdata->skybox, perspmat, GX_PERSPECTIVE);
  skybox_render (rdata->skybox);*/
  screenspace_rect (rdata->plain_texture_shader, GX_VTXFMT0, 0);
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
