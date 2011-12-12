#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <math.h>

#include "world.h"
#include "tentacles.h"
#include "screenspace.h"
#include "rendertarget.h"
#include "sintab.h"

tentacle_data tentacle_data_0;

#include "objects/tentacles.inc"

INIT_OBJECT (tentacles_obj, tentacles);

#include "objects/cross-cube.inc"

INIT_OBJECT (cross_cube_obj, cross_cube);

#define TEX_WIDTH 640
#define TEX_HEIGHT 480
#define TEX_FMT GX_TF_I8

static void
tentacle_lighting (void *privdata)
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
channel_split (void *dummy)
{
  #include "channelsplit.inc"
  GX_SetTevKColor (0, (GXColor) { 255, 0, 0 });
  GX_SetTevKColor (1, (GXColor) { 0, 255, 0 });
  GX_SetTevKColor (2, (GXColor) { 0, 0, 255 });
}

static void
tentacle_init_effect (void *params)
{
  tentacle_data *tdata = (tentacle_data *) params;
  
  tdata->tentacle_wavey_pos = memalign (32, sizeof (tentacles_pos));
  
  tdata->world = create_world (1);
  world_set_perspective (tdata->world, 60, 1.33f, 10.0f, 500.0f);
  world_set_pos_lookat_up (tdata->world,
			   (guVector) { 0, 50, -60 },
			   (guVector) { 0, 0, 0 },
			   (guVector) { 0, 1, 0 });
  world_set_light_pos_lookat_up (tdata->world, 0,
				 (guVector) { 20, 20, 30 },
				 (guVector) { 0, 0, 0 },
				 (guVector) { 0, 1, 0 });

  tdata->tentacle_shader = create_shader (&tentacle_lighting,
					  &tdata->world->light[0]);
  
  world_add_standard_object (tdata->world, &tentacles_obj,
			     &tdata->tentacle_loc, OBJECT_POS | OBJECT_NORM,
			     GX_VTXFMT0, 0, &tdata->tentacle_modelview,
			     &tdata->tentacle_scale, tdata->tentacle_shader);

  world_add_standard_object (tdata->world, &cross_cube_obj,
  			     &tdata->tentacle_loc, OBJECT_POS | OBJECT_NORM,
			     GX_VTXFMT0, 0, &tdata->tentacle_modelview,
			     &tdata->box_scale, tdata->tentacle_shader);

  tdata->back_buffer = memalign (32, GX_GetTexBufferSize (TEX_WIDTH, TEX_HEIGHT,
				 TEX_FMT, GX_FALSE, 0));
  
  GX_InitTexObj (&tdata->backbuffer_texobj, tdata->back_buffer, TEX_WIDTH,
		 TEX_HEIGHT, TEX_FMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_InitTexObjFilterMode (&tdata->backbuffer_texobj, GX_LINEAR, GX_LINEAR);

  tdata->channelsplit_shader = create_shader (&channel_split, NULL);
  shader_append_texmap (tdata->channelsplit_shader, &tdata->backbuffer_texobj,
  			GX_TEXMAP0);
  shader_append_texcoordgen (tdata->channelsplit_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
  shader_append_texcoordgen (tdata->channelsplit_shader, GX_TEXCOORD1,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX1);
  shader_append_texcoordgen (tdata->channelsplit_shader, GX_TEXCOORD2,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX2);
}


static void
tentacle_uninit_effect (void *params)
{
  tentacle_data *tdata = (tentacle_data *) params;

  free_shader (tdata->channelsplit_shader);
  free (tdata->back_buffer);
  free_shader (tdata->tentacle_shader);
  free_world (tdata->world);
  free (tdata->tentacle_wavey_pos);
}

static void
tentacle_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  tentacle_data *tdata = (tentacle_data *) params;
  Mtx rotmtx;
  guVector around = { 0, 1, 0 };
  int verts;

  for (verts = 0; verts < ARRAY_SIZE (tentacles_pos) / 3; verts++)
    {
      int idx = verts * 3;
      float amt = (tentacles_pos[idx + 1] + 8) / 16.0;
      float phase = tentacles_pos[idx + 1] + tdata->wave;
      float phase2 = 3.0 * FASTSIN (tdata->wave / 2.0);
      float x, y, z;
      
      x = tentacles_pos[idx] + amt * FASTSIN (phase);
      y = tentacles_pos[idx + 1];
      z = tentacles_pos[idx + 2];
      
      tdata->tentacle_wavey_pos[idx]
        = x * FASTCOS (amt * phase2) - z * FASTSIN (amt * phase2);
      tdata->tentacle_wavey_pos[idx + 1] = y;
      tdata->tentacle_wavey_pos[idx + 2]
        = x * FASTSIN (amt * phase2) + z * FASTCOS (amt * phase2);
    }

  /* Flush that shit out!  */
  DCFlushRange (tdata->tentacle_wavey_pos, sizeof (tentacles_pos));
  
  guMtxIdentity (tdata->tentacle_modelview);
  guMtxRotAxisDeg (rotmtx, &around, tdata->rot);
  guMtxConcat (rotmtx, tdata->tentacle_modelview, tdata->tentacle_modelview);
  
  guMtxScale (tdata->tentacle_scale, 5.0, 5.0, 5.0);
  
  guMtxScale (tdata->box_scale, 50.0, 50.0, 50.0);
  
  rendertarget_texture (TEX_WIDTH, TEX_HEIGHT, GX_CTF_R8);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  tentacles_obj.positions = tdata->tentacle_wavey_pos;
  world_display (tdata->world);
  
  GX_CopyTex (tdata->back_buffer, GX_TRUE);
  GX_PixModeSync ();
}

static void
tentacle_display_effect (uint32_t time_offset, void *params, int iparam,
			 GXRModeObj *rmode)
{
  tentacle_data *tdata = (tentacle_data *) params;
  Mtx texoffset;
  
  guMtxIdentity (texoffset);
  guMtxTransApply (texoffset, texoffset, cosf (tdata->rot2) / 50.0,
		   sinf (tdata->rot2) / 50.0, 0);
  GX_LoadTexMtxImm (texoffset, GX_TEXMTX0, GX_MTX2x4);

  guMtxIdentity (texoffset);
  guMtxTransApply (texoffset, texoffset, cosf (tdata->rot3) / 50.0,
		   sinf (tdata->rot3) / 50.0, 0);
  GX_LoadTexMtxImm (texoffset, GX_TEXMTX1, GX_MTX2x4);

  guMtxIdentity (texoffset);
  guMtxTransApply (texoffset, texoffset, cosf (tdata->wave) / 50.0,
		   sinf (tdata->wave) / 50.0, 0);
  GX_LoadTexMtxImm (texoffset, GX_TEXMTX2, GX_MTX2x4);
  
  screenspace_rect (tdata->channelsplit_shader, GX_VTXFMT1, 0);
  
  tdata->rot++;
  tdata->rot2 += 0.3;
  tdata->rot3 += 0.35;
  tdata->wave += 0.1;
}

effect_methods tentacle_methods =
{
  .preinit_assets = NULL,
  .init_effect = &tentacle_init_effect,
  .prepare_frame = &tentacle_prepare_frame,
  .display_effect = &tentacle_display_effect,
  .uninit_effect = &tentacle_uninit_effect,
  .finalize = NULL
};
