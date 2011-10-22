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

#include "objects/textured-cube.inc"

INIT_OBJECT (plane_obj, tex_cube);

static Mtx44 perspmat;

static scene_info scene =
{
  .pos = { 0, 0, 30 },
  .up = { 0, 1, 0 },
  .lookat = { 0, 0, 0 },
  .camera_dirty = 1
};

static object_loc obj_loc;

#undef USE_GRID

#ifdef USE_GRID
#include "images/grid.h"
#include "grid_tpl.h"
#else
#include "images/more_stones.h"
#include "more_stones_tpl.h"
#endif

extern TPLFile stone_textureTPL;

#ifdef USE_GRID
#include "images/height.h"
#include "height_tpl.h"
#else
#include "images/fake_stone_depth.h"
#include "fake_stone_depth_tpl.h"
#endif

static TPLFile stone_depthTPL;

static void
parallax_mapping_init_effect (void *params)
{
  guPerspective (perspmat, 30, 1.33f, 10.0f, 500.0f);
  
  object_loc_initialise (&obj_loc, GX_PNMTX0);
  object_set_parallax_tex_mtx (&obj_loc, GX_TEXMTX0, GX_TEXMTX1);
#ifdef USE_GRID
  TPL_OpenTPLFromMemory (&stone_textureTPL, (void *) grid_tpl,
			 grid_tpl_size);
  TPL_OpenTPLFromMemory (&stone_depthTPL, (void *) height_tpl,
			 height_tpl_size);
#else
  TPL_OpenTPLFromMemory (&stone_textureTPL, (void *) more_stones_tpl,
			 more_stones_tpl_size);
  TPL_OpenTPLFromMemory (&stone_depthTPL, (void *) fake_stone_depth_tpl,
			 fake_stone_depth_tpl_size);
#endif
}

static void
texturing (void)
{
#include "parallax.inc"
}

static float phase = 0;
static float phase2 = 0;

static void
parallax_mapping_display_effect (uint32_t time_offset, void *params, int iparam,
				 GXRModeObj *rmode)
{
  GXTexObj stone_tex_obj, stone_depth_obj;
  Mtx modelview, rot;
  Mtx scale;

  TPL_GetTexture (&stone_textureTPL, stone_texture, &stone_tex_obj);
  TPL_GetTexture (&stone_depthTPL, stone_depth, &stone_depth_obj);
  
  GX_InitTexObjMaxAniso (&stone_tex_obj, GX_ANISO_4);
  
  GX_InitTexObjWrapMode (&stone_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjWrapMode (&stone_depth_obj, GX_CLAMP, GX_CLAMP);
  
  GX_LoadTexObj (&stone_tex_obj, GX_TEXMAP0);
  GX_LoadTexObj (&stone_depth_obj, GX_TEXMAP1);
  
  scene_update_camera (&scene);
  
  phase += (float) PAD_StickX (0) / 100.0;
  phase2 += (float) PAD_StickY (0) / 100.0;
  
  guMtxIdentity (modelview);
  guMtxRotAxisDeg (rot, &((guVector) { 0, 1, 0 }), phase);
  guMtxConcat (rot, modelview, modelview);
  guMtxRotAxisDeg (rot, &((guVector) { 1, 0, 0 }), phase2);
  guMtxConcat (rot, modelview, modelview);
  
  guMtxScale (scale, 10.0, 10.0, 10.0);
  
  scene_update_matrices (&scene, &obj_loc, scene.camera, modelview, scale,
			 perspmat, GX_PERSPECTIVE);
  
  object_set_arrays (&plane_obj, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX0);
  GX_SetTexCoordGen (GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX1);
  GX_SetCurrentMtx (GX_PNMTX0);
  
  texturing ();
  //GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
  {
    f32 indmtx[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    guVector norm = { 0, 0, -1 };
    guVector eye = { 0, 0, -1 };
    /*float ang, dp;*/
#if 0
    int scale = -6;
    guVecMultiply (modelview, &norm, &norm);
    /*dp = guVecDotProduct (&norm, &eye);
    ang = acosf (dp);*/
    
    indmtx[0][0] = -norm.y / norm.z; // * tanf (ang);
    indmtx[0][1] = 0;
    indmtx[0][2] = 0;
    indmtx[1][0] = 0;
    indmtx[1][1] = norm.x / norm.z; // * tanf (ang);
    indmtx[1][2] = 0;
    
    for (;;)
      {
        unsigned int i, j;
	int gt_one = 0;
	
	for (i = 0; i < 2; i++)
	  for (j = 0; j < 3; j++)
	    if (fabs (indmtx[i][j]) >= 1.0)
	      gt_one = 1;
	
	if (gt_one)
	  {
	    for (i = 0; i < 2; i++)
	      for (j = 0; j < 3; j++)
		indmtx[i][j] *= 0.5;
	    scale++;
	  }
	else
	  break;
      }
    
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, scale);
#else
    int scale;
# ifdef USE_GRID
    scale = -2;
# else
    scale = -3;
# endif
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, scale);
#endif
  }
  
  object_render (&plane_obj, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		 GX_VTXFMT0);
}

effect_methods parallax_mapping_methods =
{
  .preinit_assets = NULL,
  .init_effect = &parallax_mapping_init_effect,
  .prepare_frame = NULL,
  .display_effect = &parallax_mapping_display_effect,
  .uninit_effect = NULL,
  .finalize = NULL
};
