#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

#include "timing.h"
#include "tubes.h"
#include "light.h"

#define NUM_TUBES 9

#define TUBE_AROUND 16
#define TUBE_ALONG 64

static f32 *tube[NUM_TUBES] = { 0 };
static f32 *tubenorms[NUM_TUBES] = { 0 };

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .tpos = { 0, 0, 0 },
  .lookat = { 0, 0, 0 },
  .tlookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static float lightdeg = 0.0;
static float deg = 0.0;
static float deg2 = 0.0;

static Mtx44 perspmat;
static Mtx viewmat, depth, texproj;
static guVector pos = {0, 0, 50};
static guVector up = {0, 1, 0};
static guVector lookat = {0, 0, 0};

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
  float phase2 = bigger_offset;
  
  assert (tube[which] != NULL);
  assert (tubenorms[which] != NULL);
  
  for (i = 0; i < along_steps; i++)
    {
      float x_pos = ((float) i / (float) (along_steps - 1)) * 100.0 - 50.0;
      float y_offset = 6 * cos (phase1 + x_pos * 0.2);
      float z_offset = 6 * sin (phase1 + x_pos * 0.2);
      guVector along, up = { 0.0, 1.0, 0.0 }, side;
      Mtx circ_mat;
      
      y_offset += 15 * cos (phase2 + x_pos * 3.0 / 50.0);
      z_offset += 15 * sin (phase2 + x_pos * 3.0 / 50.0);
      
      /* Differentiate the offset to get a gradient along y & z...  */
      along.x = 1.0;
      along.y = -(6.0 / 5.0) * sin (x_pos * 0.2 + phase1)
		- 0.9 * sin (x_pos * 3.0 / 50.0 + phase2);
      along.z = (6.0 / 5.0) * cos (x_pos * 0.2 + phase1)
		+ 0.9 * cos (x_pos * 3.0 / 50.0 + phase2);
      
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
	  float z_pos = radius * cos (angle);
	  float y_pos = radius * sin (angle);
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

static void
render_tube (unsigned int around_steps, unsigned int along_steps)
{
  unsigned int i;
  
  for (i = 0; i < along_steps - 1; i++)
    {
      unsigned int j;

      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, around_steps * 2 + 2);

      for (j = 0; j < around_steps; j++)
        {
	  GX_Position1x16 ((i + 1) * around_steps + j);
	  GX_Normal1x16 ((i + 1) * around_steps + j);
	  GX_Position1x16 (i * around_steps + j);
	  GX_Normal1x16 (i * around_steps + j);
	}

      GX_Position1x16 ((i + 1) * around_steps);
      GX_Normal1x16 ((i + 1) * around_steps);
      GX_Position1x16 (i * around_steps);
      GX_Normal1x16 (i * around_steps);

      GX_End ();
    }
}

static void
tubes_init_effect (void *params)
{
  unsigned int i;

  guPerspective (perspmat, 60, 1.33f, 10.0f, 300.0f);
  guLookAt (viewmat, &pos, &up, &lookat);

  for (i = 0; i < NUM_TUBES; i++)
    allocate_tube_arrays (i, TUBE_AROUND, TUBE_ALONG);
}

static void
tubes_uninit_effect (void *params)
{
  unsigned int i;

  for (i = 0; i < NUM_TUBES; i++)
    free_tube_arrays (i);
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
tubes_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  unsigned int i;

  GX_InvVtxCache ();
  GX_InvalidateTexAll ();
  
  light0.pos.x = cos ((lightdeg * 4) / 180.0 * M_PI) * 30.0;
  light0.pos.y = cos (lightdeg / 180.0 * M_PI) * 25.0;
  light0.pos.z = sin (lightdeg / 180.0 * M_PI) * 25.0;

  guVecMultiply (viewmat, &light0.pos, &light0.tpos);
  guVecMultiply (viewmat, &light0.lookat, &light0.tlookat);

  for (i = 0; i < NUM_TUBES; i++)
    fill_tube_coords (i, 2, TUBE_AROUND, TUBE_ALONG);
  
  update_anim ();
}

static void
specular_lighting_1light (void)
{
  GXLightObj lo;
  guVector ldir;

#include "tube-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPEC);

  GX_SetChanCtrl (GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
		  GX_DF_CLAMP, GX_AF_NONE);

  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  /* Light 0: use for both specular and diffuse lighting.  */
  GX_InitSpecularDir (&lo, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo, 64);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

static void
tubes_display_effect (uint32_t time_offset, void *params, int iparam,
		      GXRModeObj *rmode)
{
  unsigned int i;

  GX_SetCullMode (GX_CULL_BACK);
  GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
  GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
  
  GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);
  
  specular_lighting_1light ();
  
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  setup_tube_mats (viewmat, depth, texproj, 1);

  for (i = 0; i < NUM_TUBES; i++)
    {
      GX_SetArray (GX_VA_POS, tube[i], 3 * sizeof (f32));
      GX_SetArray (GX_VA_NRM, tubenorms[i], 3 * sizeof (f32));
      render_tube (TUBE_AROUND, TUBE_ALONG);
    }
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
