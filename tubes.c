#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

#include "timing.h"
#include "tubes.h"
#include "light.h"
#include "sintab.h"
#include "server.h"
#include "rendertarget.h"

#include "images/snakeskin.h"
#include "snakeskin_tpl.h"

static TPLFile snakeskinTPL;

#define NUM_TUBES 9

#define TUBE_AROUND 16
#define TUBE_ALONG 64

#define SHADOWBUF_W 512
#define SHADOWBUF_H 512
#define SHADOWBUF_FMT GX_TF_IA8

#define NUM_SHADOWS 1

tube_data tube_data_0;

static f32 *tube[NUM_TUBES] = { 0 };
static f32 *tubenorms[NUM_TUBES] = { 0 };

static float lightdeg = 0.0;
static float deg = 0.0;
static float deg2 = 0.0;

//static Mtx viewmat, depth, texproj;

static void
allocate_tube_arrays (unsigned int which, unsigned int around_steps,
		      unsigned int along_steps)
{
  tube[which] = memalign (32, around_steps * along_steps * 3 * sizeof (f32));
  tubenorms[which] = memalign (32, around_steps * along_steps * 3
			       * sizeof (f32));
}

static void
free_tube_arrays (unsigned int which)
{
  free (tube[which]);
  free (tubenorms[which]);
  tube[which] = NULL;
  tubenorms[which] = NULL;
}

static float phase = 0.0;

/* A tube around X axis, from -1 to 1, with radius RADIUS.  */

static void
fill_tube_coords (unsigned int which, float radius, unsigned int around_steps,
		  unsigned int along_steps)
{
  int i, j;
  float which_phase_offset = 2.0 * M_PI * (which % 3) / 3.0;
  float bigger_offset = 2.0 * M_PI * (which / 3) / 3.0;
  float phase1 = which_phase_offset + phase;
  float phase2 = bigger_offset - phase / 2.5;
  
  /* Override...  */
  /*phase1 = which_phase_offset + (phase / 20.0);
  phase2 = bigger_offset - phase / (2.5 * 20.0);*/
  
  assert (tube[which] != NULL);
  assert (tubenorms[which] != NULL);
  
  for (i = 0; i < along_steps; i++)
    {
      float x_pos = ((float) i / (float) (along_steps - 1)) * 100.0 - 50.0;
      float y_offset = 6 * FASTCOS (phase1 + x_pos * 0.2);
      float z_offset = 6 * FASTSIN (phase1 + x_pos * 0.2);
      guVector along, up = { 0.0, 1.0, 0.0 }, side;
      Mtx circ_mat;
      
      y_offset += 15 * FASTCOS (phase2 + x_pos * 3.0 / 50.0);
      z_offset += 15 * FASTSIN (phase2 + x_pos * 3.0 / 50.0);
      
      /* Differentiate the offset to get a gradient along y & z...  */
      along.x = 1.0;
      along.y = -(6.0 / 5.0) * FASTSIN (x_pos * 0.2 + phase1)
		- 0.9 * FASTSIN (x_pos * 3.0 / 50.0 + phase2);
      along.z = (6.0 / 5.0) * FASTCOS (x_pos * 0.2 + phase1)
		+ 0.9 * FASTCOS (x_pos * 3.0 / 50.0 + phase2);
      
      guVecCross (&along, &up, &side);
      guVecNormalize (&side);
      guVecCross (&side, &along, &up);
      guVecNormalize (&up);
      guVecCross (&up, &side, &along);
      
      guMtxRowCol (circ_mat, 0, 0) = along.x;
      guMtxRowCol (circ_mat, 1, 0) = along.y;
      guMtxRowCol (circ_mat, 2, 0) = along.z;
      guMtxRowCol (circ_mat, 0, 1) = up.x;
      guMtxRowCol (circ_mat, 1, 1) = up.y;
      guMtxRowCol (circ_mat, 2, 1) = up.z;
      guMtxRowCol (circ_mat, 0, 2) = side.x;
      guMtxRowCol (circ_mat, 1, 2) = side.y;
      guMtxRowCol (circ_mat, 2, 2) = side.z;
      guMtxRowCol (circ_mat, 0, 3) = 0;
      guMtxRowCol (circ_mat, 1, 3) = 0;
      guMtxRowCol (circ_mat, 2, 3) = 0;
      
      for (j = 0; j < around_steps; j++)
        {
	  float angle = (float) j * 2 * M_PI / (float) around_steps;
	  float z_pos = radius * FASTCOS (angle);
	  float y_pos = radius * FASTSIN (angle);
	  unsigned int pt_idx = (i * around_steps + j) * 3;
	  guVector circ_point;
	  
	  circ_point.x = 0;
	  circ_point.y = y_pos;
	  circ_point.z = z_pos;
	  
	  guVecMultiplySR (circ_mat, &circ_point, &circ_point);
	  
	  tube[which][pt_idx] = x_pos + circ_point.x;
	  tube[which][pt_idx + 1] = y_offset + circ_point.y;
	  tube[which][pt_idx + 2] = z_offset + circ_point.z;
	  
	  guVecNormalize (&circ_point);
	  
	  tubenorms[which][pt_idx] = circ_point.x;
	  tubenorms[which][pt_idx + 1] = circ_point.y;
	  tubenorms[which][pt_idx + 2] = circ_point.z;
	}
    }
  
  DCFlushRange (tube[which], around_steps * along_steps * 3 * sizeof (f32));
  DCFlushRange (tubenorms[which],
		around_steps * along_steps * 3 * sizeof (f32));
}

#if 0
static void
setup_tube_mats (Mtx viewMatrix, Mtx depth, Mtx texproj, int do_texture_mats)
{
  Mtx modelView, mvi, mvitmp, scale, rotmtx;
  guVector axis = {0, 1, 0};
  guVector axis2 = {0, 0, 1};

  guMtxIdentity (modelView);
  guMtxRotAxisDeg (modelView, &axis, deg);
  guMtxRotAxisDeg (rotmtx, &axis2, deg2);

  guMtxConcat (modelView, rotmtx, modelView);

  guMtxScale (scale, 1.0, 1.0, 1.0);
  guMtxConcat (modelView, scale, modelView);

  if (do_texture_mats)
    {
      guMtxConcat (depth, modelView, mvitmp);
      GX_LoadTexMtxImm (mvitmp, GX_TEXMTX0, GX_MTX3x4);
      guMtxConcat (texproj, modelView, mvitmp);
      GX_LoadTexMtxImm (mvitmp, GX_TEXMTX1, GX_MTX3x4);
    }

  //guMtxTransApply (modelView, modelView, 0.0F, 0.0F, 0.0F);
  guMtxConcat (viewMatrix, modelView, modelView);

  GX_LoadPosMtxImm (modelView, GX_PNMTX0);
  guMtxInverse (modelView, mvitmp);
  guMtxTranspose (mvitmp, mvi);
  GX_LoadNrmMtxImm (mvi, GX_PNMTX0);
}
#endif

static void
render_tube (unsigned int around_steps, unsigned int along_steps)
{
  unsigned int i;
  
  for (i = 0; i < along_steps - 1; i++)
    {
      unsigned int j;
      float tex_t = (float) i / (float) along_steps;
      float tex_t_1 = (float) (i + 1) / (float) along_steps;

      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, around_steps * 2 + 2);

      for (j = 0; j < around_steps; j++)
        {
	  float tex_s = (float) j / (float) around_steps;

	  GX_Position1x16 ((i + 1) * around_steps + j);
	  GX_Normal1x16 ((i + 1) * around_steps + j);
	  GX_TexCoord2f32 (tex_s, tex_t_1 * 3);
	  GX_Position1x16 (i * around_steps + j);
	  GX_Normal1x16 (i * around_steps + j);
	  GX_TexCoord2f32 (tex_s, tex_t * 3);
	}

      GX_Position1x16 ((i + 1) * around_steps);
      GX_Normal1x16 ((i + 1) * around_steps);
      GX_TexCoord2f32 (0.0, tex_t_1 * 3);
      GX_Position1x16 (i * around_steps);
      GX_Normal1x16 (i * around_steps);
      GX_TexCoord2f32 (0.0, tex_t * 3);

      GX_End ();
    }
}

static void
tube_shader_setup (void *privdata)
{
  world_info *world = (world_info *) privdata;
  GXLightObj lo0;
  GXLightObj lo1;
  guVector ldir;

#include "tube-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 192, 192, 192, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT0 /*| GX_LIGHT1*/, GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT0 /*| GX_LIGHT1*/, GX_DF_CLAMP, GX_AF_SPEC);

  /* Light 0.  */
  guVecSub (&world->light[0].tpos, &world->light[0].tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightDistAttn (&lo0, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  /* Light 1.  */
  GX_SetChanAmbColor (GX_COLOR1, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_COLOR1, (GXColor) { 64, 64, 64, 0 });
  GX_SetChanCtrl (GX_COLOR1, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT1, GX_DF_CLAMP, GX_AF_NONE);
  GX_SetChanCtrl (GX_ALPHA1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG,
		  0, GX_DF_CLAMP, GX_AF_NONE);

  GX_InitLightPos (&lo1, world->light[1].tpos.x, world->light[1].tpos.y,
		   world->light[1].tpos.z);
  GX_InitLightDistAttn (&lo1, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightColor (&lo1, (GXColor) { 128, 32, 32, 192 });
  GX_LoadLightObj (&lo1, GX_LIGHT1);
  
  GX_SetTevKColor (GX_KCOLOR0, (GXColor) { 24, 24, 24, 0 });
}

static void
tubes_init_effect (void *params, backbuffer_info *bbuf)
{
  unsigned int i;
  tube_data *tdata = (tube_data *) params;

  tdata->world = create_world (2);
  
  world_set_perspective (tdata->world, 60, 1.33f, 10.0f, 300.0f);
  world_set_pos_lookat_up (tdata->world,
			   (guVector) { 0, 0, 50 },
			   (guVector) { 0, 0, 0 },
			   (guVector) { 0, 1, 0 });
  world_set_light_pos_lookat_up (tdata->world, 0,
				 (guVector) { 20, 20, 30 },
				 (guVector) { 0, 0, 0 },
				 (guVector) { 0, 1, 0 });
  world_set_light_pos_lookat_up (tdata->world, 1,
				 (guVector) { -20, -20, 30 },
				 (guVector) { 0, 0, 0 },
				 (guVector) { 0, 1, 0 });

  for (i = 0; i < NUM_TUBES; i++)
    allocate_tube_arrays (i, TUBE_AROUND, TUBE_ALONG);
  
  TPL_OpenTPLFromMemory (&snakeskinTPL, (void *) snakeskin_tpl,
			 snakeskin_tpl_size);

  TPL_GetTexture (&snakeskinTPL, snakeskin, &tdata->snakeskin_texture_obj);

  object_loc_initialise (&tdata->tube_locator, GX_PNMTX0);
#if NUM_SHADOWS == 2
  object_loc_initialise (&tdata->tube_locator_2, GX_PNMTX0);
  object_set_chained_loc (&tdata->tube_locator, &tdata->tube_locator_2);
#endif
  
  for (i = 0; i < NUM_SHADOWS; i++)
    {
      tdata->shadow[i].info = create_shadow_info (16, &tdata->world->light[i]);
      tdata->shadow[i].buf = memalign (32, GX_GetTexBufferSize (SHADOWBUF_W,
				       SHADOWBUF_H, SHADOWBUF_FMT, GX_FALSE,
				       0));
      GX_InitTexObj (&tdata->shadow[i].buf_texobj, tdata->shadow[i].buf,
		     SHADOWBUF_W, SHADOWBUF_H, SHADOWBUF_FMT, GX_CLAMP,
		     GX_CLAMP, GX_FALSE);
      GX_InitTexObjFilterMode (&tdata->shadow[i].buf_texobj, GX_NEAR, GX_NEAR);

      shadow_set_bounding_radius (tdata->shadow[i].info, 55);
      shadow_setup_ortho (tdata->shadow[i].info, 20, 400);
    }
  /*GX_InitTexObjMinLOD (&tdata->shadowbuf_obj, 1);
  GX_InitTexObjMaxLOD (&tdata->shadowbuf_obj, 1);*/

  tdata->tube_shader = create_shader (&tube_shader_setup,
				      (void *) tdata->world);
  shader_append_texmap (tdata->tube_shader, &tdata->snakeskin_texture_obj,
			GX_TEXMAP0);
  shader_append_texmap (tdata->tube_shader,
			get_utility_texture (tdata->shadow[0].info->ramp_type),
			GX_TEXMAP1);
  shader_append_texmap (tdata->tube_shader, &tdata->shadow[0].buf_texobj,
			GX_TEXMAP2);
  shader_append_texcoordgen (tdata->tube_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);
  shader_append_texcoordgen (tdata->tube_shader, GX_TEXCOORD1, GX_TG_MTX3x4,
			     GX_TG_POS, GX_TEXMTX0);
  shader_append_texcoordgen (tdata->tube_shader, GX_TEXCOORD2, GX_TG_MTX3x4,
			     GX_TG_POS, GX_TEXMTX1);
  shader_append_texcoordgen (tdata->tube_shader, GX_TEXCOORD3, GX_TG_MTX3x4,
			     GX_TG_POS, GX_TEXMTX2);
  shader_append_texcoordgen (tdata->tube_shader, GX_TEXCOORD4, GX_TG_MTX3x4,
			     GX_TG_POS, GX_TEXMTX3);

  /* TEXMTX0 is ramp texture lookup, TEXMTX{1,2,3} for shadow buffer lookup.  */
  object_set_multisample_shadow_tex_matrix (&tdata->tube_locator, GX_TEXMTX1,
					    GX_TEXMTX2, GX_TEXMTX3, GX_TEXMTX0,
					    1./256, tdata->shadow[0].info);
#if NUM_SHADOWS == 2
  /* TEXMTX4 is ramp texture lookup, TEXMTX{5,6,7} for shadow buffer lookup.  */
  object_set_multisample_shadow_tex_matrix (&tdata->tube_locator_2, GX_TEXMTX5,
					    GX_TEXMTX6, GX_TEXMTX7, GX_TEXMTX4,
					    1./256, tdata->shadow[1].info);
#endif
}

static void
tubes_uninit_effect (void *params, backbuffer_info *bbuf)
{
  tube_data *tdata = (tube_data *) params;
  unsigned int i;

  for (i = 0; i < NUM_TUBES; i++)
    free_tube_arrays (i);

  free_world (tdata->world);
  free_shader (tdata->tube_shader);
  for (i = 0; i < NUM_SHADOWS; i++)
    {
      destroy_shadow_info (tdata->shadow[i].info);
      free (tdata->shadow[i].buf);
    }
}

static void
update_anim (void)
{
  deg++;
  lightdeg += 0.5;
  deg2 += 0.5;
  phase += 0.1;
}

static void
render_tubes (void)
{
  int i;

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX16);
  GX_SetVtxDesc (GX_VA_NRM, GX_INDEX16);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  for (i = 0; i < NUM_TUBES; i++)
    {
      GX_SetArray (GX_VA_POS, tube[i], 3 * sizeof (f32));
      GX_SetArray (GX_VA_NRM, tubenorms[i], 3 * sizeof (f32));
      render_tube (TUBE_AROUND, TUBE_ALONG);
    }
}

/*static int alternate = 0;*/

static display_target
tubes_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  unsigned int i;
  tube_data *tdata = (tube_data *) params;
  world_info *world = tdata->world;
  scene_info *scene = &world->scene;
  object_loc shadowcast_loc;
  Mtx rotmtx;
  const guVector axis = {0, 1, 0};
  const guVector axis2 = {0, 0, 1};
  /*extern int switch_ghost_lighting;*/

  GX_InvVtxCache ();
  
  tdata->world->light[0].pos.x = FASTCOS ((lightdeg * 4) / 180.0 * M_PI)
				 * 120.0;
  tdata->world->light[0].pos.y = FASTCOS (lightdeg / 180.0 * M_PI) * 100.0;
  tdata->world->light[0].pos.z = FASTSIN ((lightdeg * 4) / 180.0 * M_PI)
				 * 100.0;

  /*if (switch_ghost_lighting)
    tdata->world->light[0].pos.x = 120.0;
  else
    tdata->world->light[0].pos.x = -120.0;
  tdata->world->light[0].pos.y = 0.0;
  tdata->world->light[0].pos.z = 0.0;*/

  /*light_update (viewmat, &light0);
  light_update (viewmat, &light1);*/

  for (i = 0; i < NUM_TUBES; i++)
    fill_tube_coords (i, 2, TUBE_AROUND, TUBE_ALONG);
  
  world_display (world);

  object_loc_initialise (&shadowcast_loc, GX_PNMTX0);

  /*i = alternate;
  alternate = 1 - alternate;*/

  for (i = 0; i < NUM_SHADOWS; i++)
    {
      /* There isn't a way to update the light matrices! Re-running this is a
         bit ugly. FIXME.  */
      shadow_setup_ortho (tdata->shadow[i].info, 20, 400);

      guMtxRotAxisDeg (tdata->modelview, (guVector *) &axis, deg);
      guMtxRotAxisDeg (rotmtx, (guVector *) &axis2, deg);

      guMtxConcat (tdata->modelview, rotmtx, tdata->modelview);

      rendertarget_texture (SHADOWBUF_W, SHADOWBUF_H, GX_TF_Z16, GX_FALSE,
			    GX_PF_Z24, GX_ZC_LINEAR);
      GX_SetCullMode (GX_CULL_FRONT);

      shadow_casting_tev_setup (NULL);
      object_set_matrices (scene, &shadowcast_loc,
			   tdata->shadow[i].info->light_cam,
			   tdata->modelview, NULL,
			   tdata->shadow[i].info->shadow_projection,
			   tdata->shadow[i].info->projection_type);

      GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
      GX_SetColorUpdate (GX_FALSE);
      GX_SetAlphaUpdate (GX_FALSE);

      render_tubes ();

      GX_CopyTex (tdata->shadow[i].buf
		  /*+ GX_GetTexBufferSize (SHADOWBUF_W, SHADOWBUF_H, SHADOWBUF_FMT,
					 GX_FALSE, 0)*/, GX_TRUE);
    }

  GX_PixModeSync ();
    
  update_anim ();
  
  return MAIN_BUFFER;
}

static void
tubes_display_effect (uint32_t time_offset, void *params, int iparam)
{
  tube_data *tdata = (tube_data *) params;
  world_info *world = tdata->world;

  //GX_InvalidateTexAll ();
  
  /* Updates various things, transformed light positions etc.  */
  world_display (world);
  
  //GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);
  
  shader_load (tdata->tube_shader);
  
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);
  GX_SetCullMode (GX_CULL_BACK);

  object_set_matrices (&world->scene, &tdata->tube_locator,
		       world->scene.camera, tdata->modelview, NULL,
		       world->projection, world->projection_type);
  /*light_update (tdata->shadow_info_0->light_cam, &world->light[0]);
  object_set_matrices (&world->scene, &tdata->tube_locator,
		       tdata->shadow_info_0->light_cam, tdata->modelview, NULL,
		       tdata->shadow_info_0->shadow_projection,
		       tdata->shadow_info_0->projection_type);*/
  //setup_tube_mats (viewmat, depth, texproj, 1);
  
  render_tubes ();
}

effect_methods tubes_methods =
{
  .preinit_assets = NULL,
  .init_effect = &tubes_init_effect,
  .prepare_frame = &tubes_prepare_frame,
  .display_effect = &tubes_display_effect,
  .uninit_effect = &tubes_uninit_effect,
  .finalize = NULL
};
