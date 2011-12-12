#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <math.h>

#include "world.h"
#include "tentacles.h"

tentacle_data tentacle_data_0;

#include "objects/tentacles.inc"

INIT_OBJECT (tentacles_obj, tentacles);

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
}

static void
tentacle_uninit_effect (void *params)
{
  tentacle_data *tdata = (tentacle_data *) params;

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
      
      tdata->tentacle_wavey_pos[idx] = tentacles_pos[idx] + amt * sin (phase);
      tdata->tentacle_wavey_pos[idx + 1] = tentacles_pos[idx + 1];
      tdata->tentacle_wavey_pos[idx + 2] = tentacles_pos[idx + 2];
    }

  /* Flush that shit out!  */
  DCFlushRange (tdata->tentacle_wavey_pos, sizeof (tentacles_pos));
  
  guMtxIdentity (tdata->tentacle_modelview);
  guMtxRotAxisDeg (rotmtx, &around, tdata->rot);
  guMtxConcat (rotmtx, tdata->tentacle_modelview, tdata->tentacle_modelview);
  
  guMtxScale (tdata->tentacle_scale, 5.0, 5.0, 5.0);
  
  tentacles_obj.positions = tdata->tentacle_wavey_pos;
  world_display (tdata->world);
}

static void
tentacle_display_effect (uint32_t time_offset, void *params, int iparam,
			 GXRModeObj *rmode)
{
  tentacle_data *tdata = (tentacle_data *) params;
  
  
  
  tdata->rot++;
  tdata->wave += 0.1;
}

effect_methods tentacle_methods =
{
  .preinit_assets = NULL,
  .init_effect = &tentacle_init_effect,
  .prepare_frame = NULL,
  .display_effect = &tentacle_display_effect,
  .uninit_effect = &tentacle_uninit_effect,
  .finalize = NULL
};
