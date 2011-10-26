#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "rendertarget.h"
#include "spooky-ghost.h"
#include "light.h"
#include "object.h"
#include "scene.h"
#include "server.h"
#include "ghost-obj.h"
#include "lighting-texture.h"

#include "images/more_stones.h"
#include "more_stones_tpl.h"

TPLFile stone_textureTPL;

#include "images/stones_bump.h"
#include "stones_bump_tpl.h"

static TPLFile stone_bumpTPL;

#include "objects/tunnel-section.inc"

INIT_OBJECT (tunnel_section_obj, tunnel_section);

#define REFLECTION_W 640
#define REFLECTION_H 480
#define REFLECTION_TEXFMT GX_TF_RGB565

#undef SEE_TEXTURES

static light_info light0 =
{
  .pos = { 0, 0, -50 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 perspmat;

static scene_info scene =
{
  .pos = { 0, 0, 30 },
  .up = { 0, 1, 0 },
  .lookat = { 0, 0, 0 },
  .camera_dirty = 1
};

static object_loc obj_loc;
static lighting_texture_info *lighting_texture;

/*static Mtx viewmat;
static guVector pos = {0, 0, 30};
static guVector up = {0, 1, 0};
static guVector lookat = {0, 0, 0};*/

static float deg = 0.0;
static float deg2 = 0.0;
static float lightdeg = 0.0;

int switch_ghost_lighting = 1;

static void
tunnel_lighting (void)
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
bump_mapping_tev_setup (void)
{
  f32 indmtx[2][3] = { { 0.5, 0.0, 0.0 },
		       { 0.0, 0.5, 0.0 } };
#include "bump-mapping.inc"
  //GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 4);
  
  GX_SetChanCtrl (GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_NONE);
  GX_SetChanCtrl (GX_ALPHA0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_NONE);
}

void *lightmap;
void *reflection;

static void
spooky_ghost_init_effect (void *params)
{
  guPerspective (perspmat, 60, 1.33f, 10.0f, 500.0f);
  scene_update_camera (&scene);
  
  object_loc_initialise (&obj_loc, GX_PNMTX0);
  object_set_tex_norm_binorm_matrices (&obj_loc, GX_TEXMTX1, GX_TEXMTX2);
  
  TPL_OpenTPLFromMemory (&stone_textureTPL, (void *) more_stones_tpl,
			 more_stones_tpl_size);
  TPL_OpenTPLFromMemory (&stone_bumpTPL, (void *) stones_bump_tpl,
			 stones_bump_tpl_size);

  lighting_texture = create_lighting_texture ();

  reflection = memalign (32, GX_GetTexBufferSize (REFLECTION_W, REFLECTION_H,
			 REFLECTION_TEXFMT, GX_FALSE, 0));
}

static void
spooky_ghost_uninit_effect (void *params)
{
  free_lighting_texture (lighting_texture);
  free (reflection);
}

static float bla = 0.0;

#if 0
#define WAVES 5

static float offset[5][64][64];

static float
height_at (float x, float y, float phase)
{
  int i, x, y;
  float ht = 0.0;
  static int initialised = 0;
  
  if (!initialised)
    {
      for (i = 0; i < WAVES; i++)
        {
	  int x_divisions = (lrand48 () % 16) + 8;
	  int y_divisions = (lrand48 () % 16) + 8;
	  float x_amp = drand48 ();
	  float y_amp = drand48 ();

	  for (x = 0; x < 64; x++)
	    {
	      for (y = 0; y < 64; y++)
	        {
		  offset[i][x][y] = x_amp * sin (x * 2 * M_PI
				    / (64.0 * x_divisions));
		  offset[i][x][y] += y_amp * sin (y * 2 * M_PI
				     / (64.0 * y_divisions));
		}
	    }
	}
      initialised = 1;
    }
  
  for (i = 0; i < WAVES; i++)
    {
      ht += x_amp[i] * sinf (phase * x_phase[i] + x_freq[i] * x);
      ht += y_amp[i] * sinf (phase * y_phase[i] + y_freq[i] * y);
    }
  
  return ht;
}

float w_phase = 0;

static void
draw_waves (void)
{
  u32 i, j;
  guVector c, cn;
  const u32 isize = 64, jsize = 64;

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);

  GX_SetCullMode (GX_CULL_NONE);

  for (i = 0; i < isize; i++)
    {
      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT1, jsize * 2 + 2);
      
      for (j = 0; j <= jsize; j++)
        {
	  float xf = (2.0 * (float) i / isize) - 1.0;
	  float yf = (2.0 * (float) j / jsize) - 1.0;
	  c.x = 100 + 100 * xf;
	  c.y = -15 + height_at (xf, yf, w_phase);
	  c.z = 100 * yf;
	  cn.x = 0;
	  cn.y = 1;
	  cn.z = 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	  
	  xf = (2.0 * (float) (i + 1) / isize) - 1.0;
	  
	  c.x = 100 + 100 * xf;
	  c.y = -15 + height_at (xf, yf, w_phase);
	  cn.x = 0;
	  cn.y = 1;
	  cn.z = 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	}
      
      GX_End ();
    }
  w_phase += 0.01;
}
#endif

static void
spooky_ghost_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  light0.pos.x = cos (lightdeg) * 500.0;
  light0.pos.y = -1000; // sin (lightdeg / 1.33) * 300.0;
  light0.pos.z = sin (lightdeg) * 500.0;
  
  lightdeg += 0.03;

  if (switch_ghost_lighting)
    {
      tunnel_lighting ();
      light_update (scene.camera, &light0);
      update_lighting_texture (&scene, lighting_texture);
    }
}

#ifdef SEE_TEXTURES

static void
draw_square (void)
{
  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

  GX_Position3f32 (1, -1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (-1, -1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (1, 1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (-1, 1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (0, 1);

  GX_End ();
}

#endif

static void
draw_reflection (void)
{
  Mtx mvtmp;
  object_loc reflection_loc;
  scene_info reflscene;
  Mtx44 ortho;

  /* "Straight" camera.  */
  scene_set_pos (&reflscene, (guVector) { 0, 0, 0 });
  scene_set_lookat (&reflscene, (guVector) { 5, 0, 0 });
  scene_set_up (&reflscene, (guVector) { 0, 1, 0 });
  scene_update_camera (&reflscene);

  guOrtho (ortho, -1, 1, -1, 1, 1, 15);
  
  object_loc_initialise (&reflection_loc, GX_PNMTX0);
  
  guMtxIdentity (mvtmp);

  scene_update_matrices (&reflscene, &reflection_loc, reflscene.camera, mvtmp,
			 NULL, ortho, GX_ORTHOGRAPHIC);

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

#include "water-texture.inc"
  //GX_SetTevColor (0, (GXColor) { 0, 0, 0 });

  GX_SetCullMode (GX_CULL_NONE);

  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT1, 4);

  GX_Position3f32 (3, -1, 1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (3, -1, -1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (3, 1, 1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (3, 1, -1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (0, 1);

  GX_End ();
}


#ifdef SEE_TEXTURES
void dummy ()
{
  /* Draw an square in space.  */
  {
    Mtx mvtmpx;
    
    guMtxTransApply (mvtmp, mvtmpx, -5.0, 12.0, 0.0);
    update_matrices (mvtmpx, viewmat);
    
    GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_ClearVtxDesc ();
    GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
    GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    
    #include "just-texture.inc"
    
    draw_square ();

    #include "alpha-texture.inc"

    guMtxTransApply (mvtmp, mvtmpx, 5.0, 12.0, 0.0);
    update_matrices (mvtmpx, viewmat);
    draw_square ();
  }
}
#endif


static void
spooky_ghost_display_effect (uint32_t time_offset, void *params, int iparam,
			     GXRModeObj *rmode)
{
  Mtx modelView, mvtmp;
  GXTexObj texture;
  GXTexObj bumpmap;
  GXTexObj reflection_obj;
  int i;

  scene_set_pos (&scene, (guVector) { bla, 0.0, 0.0 });
  scene_set_lookat (&scene, (guVector)
		    { bla + 5, cos (deg) * 0.2 + cos (deg2) * 0.1,
		      sin (deg2) * 0.2 + sin (deg) * 0.1 });

  //guLookAt (viewmat, &pos, &up, &lookat);

  TPL_GetTexture (&stone_textureTPL, stone_texture, &texture);
  TPL_GetTexture (&stone_bumpTPL, stone_bump, &bumpmap);

  GX_InitTexObj (&reflection_obj, reflection, REFLECTION_W, REFLECTION_H,
		 REFLECTION_TEXFMT, GX_CLAMP, GX_CLAMP, GX_FALSE);

  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&texture, GX_TEXMAP0);
  GX_LoadTexObj (&lighting_texture->texobj, GX_TEXMAP1);
  GX_LoadTexObj (&bumpmap, GX_TEXMAP2);
  GX_LoadTexObj (&reflection_obj, GX_TEXMAP3);

  scene_update_camera (&scene);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  guMtxIdentity (modelView);

  guMtxScaleApply (modelView, mvtmp, 2.0, 2.0, 2.0);

  light_update (scene.camera, &light0);

  guMtxIdentity (modelView);
  scene_update_matrices (&scene, &obj_loc, scene.camera, modelView, NULL,
			 perspmat, GX_PERSPECTIVE);

  /* I want it bigger, but I don't want to fuck up the normals!
     This is stupid, fix it.  */
  {
    Mtx vertex;
    
    guMtxIdentity (modelView);

    guMtxScaleApply (modelView, mvtmp, 25.0, 25.0, 25.0);
    
    guMtxConcat (scene.camera, mvtmp, vertex);

    GX_LoadPosMtxImm (vertex, GX_PNMTX0);
  }

  light_update (scene.camera, &light0);

  object_set_arrays (&tunnel_section_obj,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  if (switch_ghost_lighting)
    bump_mapping_tev_setup ();
  else
    tunnel_lighting ();
  
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX1);
  
  GX_SetCurrentMtx (GX_PNMTX0);
  
  rendertarget_texture (REFLECTION_W, REFLECTION_H, REFLECTION_TEXFMT);

  GX_SetCullMode (GX_CULL_FRONT);

  for (i = 0; i < 15; i++)
    {
      const float fudge_factor = 1.15;
      const float size = 28.0;
      Mtx vertex;

      /* Draw reflected!  */
      guMtxIdentity (modelView);

      guMtxScaleApply (modelView, mvtmp, 1, -1, 1);
      guMtxTransApply (mvtmp, mvtmp, i * fudge_factor, -1.15, 0.0);
      guMtxScaleApply (mvtmp, mvtmp, size, size, size);
      
      guMtxConcat (scene.camera, mvtmp, vertex);

      GX_LoadPosMtxImm (vertex, GX_PNMTX0);

      object_render (&tunnel_section_obj,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD, GX_VTXFMT0);
    }

  object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     0);

  guMtxIdentity (modelView);
  
  guMtxScaleApply (modelView, mvtmp, 5.0, 5.0, 5.0);
  guMtxTransApply (mvtmp, mvtmp, 64.4, -16, ((int) (bla + 32) % 32) - 16.0);
  
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp, NULL, NULL, 0);

  /* Override position to make it reflected.  (Ew!).  */
  guMtxIdentity (modelView);
  guMtxScaleApply (modelView, mvtmp, 5.0, -5.0, 5.0);
  guMtxTransApply (mvtmp, mvtmp, 64.4, -16, ((int) (bla + 32) % 32) - 16.0);
  guMtxConcat (scene.camera, mvtmp, mvtmp);
  GX_LoadPosMtxImm (mvtmp, obj_loc.pnmtx);

  tunnel_lighting ();

  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  GX_CopyTex (reflection, GX_TRUE);
  GX_PixModeSync ();

  rendertarget_screen (rmode);

  /* Might be unnecessary.  */
  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&texture, GX_TEXMAP0);
  GX_LoadTexObj (&lighting_texture->texobj, GX_TEXMAP1);
  GX_LoadTexObj (&bumpmap, GX_TEXMAP2);
  GX_LoadTexObj (&reflection_obj, GX_TEXMAP3);

  /* Draw "water".  */
#if 1
  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  draw_reflection ();
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
#else
  guMtxIdentity (modelView);
  scene_update_matrices (&scene, &obj_loc, scene.camera, modelView);
  tunnel_lighting ();
  draw_waves ();
#endif

  const float fudge_factor = 1.15;
  const float size = 28.0;

  guMtxIdentity (modelView);
  scene_update_matrices (&scene, &obj_loc, scene.camera, modelView, NULL,
			 perspmat, GX_PERSPECTIVE);

  if (switch_ghost_lighting)
    bump_mapping_tev_setup ();
  else
    tunnel_lighting ();
  
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX1);

  /* I want it bigger, but I don't want to fuck up the normals!
     This is stupid, fix it.  */
  {
    Mtx vertex;
    
    guMtxIdentity (modelView);

    guMtxScaleApply (modelView, mvtmp, 25.0, 25.0, 25.0);
    
    guMtxConcat (scene.camera, mvtmp, vertex);

    GX_LoadPosMtxImm (vertex, GX_PNMTX0);
  }

  object_set_arrays (&tunnel_section_obj,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  GX_SetCullMode (GX_CULL_BACK);

  GX_SetFog (GX_FOG_PERSP_LIN, 50, 300, 10.0, 500.0, (GXColor) { 0, 0, 0 });

  for (i = 0; i < 15; i++)
    {
      Mtx vertex;

      /* Draw right way up.  */
      guMtxIdentity (modelView);

      guMtxTransApply (modelView, mvtmp, i * fudge_factor, 0.0, 0.0);
      guMtxScaleApply (mvtmp, mvtmp, size, size, size);
      
      guMtxConcat (scene.camera, mvtmp, vertex);

      GX_LoadPosMtxImm (vertex, GX_PNMTX0);

      object_render (&tunnel_section_obj,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD, GX_VTXFMT0);
    }

  bla += 0.15;
  if (bla >= size * fudge_factor) bla -= size * fudge_factor;

  GX_SetCullMode (GX_CULL_BACK);

  object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     0);

  guMtxIdentity (modelView);
  
  guMtxScaleApply (modelView, mvtmp, 5.0, 5.0, 5.0);
  guMtxTransApply (mvtmp, mvtmp, 64.4, -14, ((int) (bla + 32) % 32) - 16.0);
  
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp, NULL, NULL, 0);

  tunnel_lighting ();

  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  deg += 0.012;
  deg2 += 0.05;
}

effect_methods spooky_ghost_methods =
{
  .preinit_assets = NULL,
  .init_effect = &spooky_ghost_init_effect,
  .prepare_frame = &spooky_ghost_prepare_frame,
  .display_effect = &spooky_ghost_display_effect,
  .uninit_effect = &spooky_ghost_uninit_effect,
  .finalize = NULL
};
