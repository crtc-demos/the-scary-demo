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

static light_info light0 =
{
  .pos = { 20, 20, 50 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static object_loc obj_loc;

#define USE_GRID
#define WITH_LIGHTING

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

static lighting_texture_info *lighting_texture;

#ifdef WITH_LIGHTING
#include "images/height_bump.h"
#include "height_bump_tpl.h"

static TPLFile height_bumpTPL;

void *texcoord_map;
void *texcoord_map2;

#define TEXCOORD_MAP_H 256
#define TEXCOORD_MAP_W 256
#define TEXCOORD_MAP_FMT GX_TF_IA8
#endif

static void
parallax_mapping_init_effect (void *params)
{
  unsigned int tex_edge_length;
  guPerspective (perspmat, 30, 1.33f, 10.0f, 500.0f);

#ifdef USE_GRID
  tex_edge_length = 256;
#else
  tex_edge_length = 1024;
#endif
  
  object_loc_initialise (&obj_loc, GX_PNMTX0);
  object_set_parallax_tex_matrices (&obj_loc, GX_TEXMTX0, GX_TEXMTX1,
				    tex_edge_length);
#ifdef WITH_LIGHTING
  object_set_tex_norm_binorm_matrices (&obj_loc, GX_TEXMTX2, GX_TEXMTX3);
#endif

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
#ifdef WITH_LIGHTING
  TPL_OpenTPLFromMemory (&height_bumpTPL, (void *) height_bump_tpl,
			 height_bump_tpl_size);
#endif

  lighting_texture = create_lighting_texture ();
  texcoord_map = memalign (32, GX_GetTexBufferSize (TEXCOORD_MAP_W,
			   TEXCOORD_MAP_H, TEXCOORD_MAP_FMT, GX_FALSE, 0));
  texcoord_map2 = memalign (32, GX_GetTexBufferSize (TEXCOORD_MAP_W,
			    TEXCOORD_MAP_H, TEXCOORD_MAP_FMT, GX_FALSE, 0));
}

static void
parallax_mapping_uninit_effect (void *params)
{
  free_lighting_texture (lighting_texture);
}

static void
texturing (int method)
{
  if (method == 0)
    {
      #include "parallax.inc"
    }
  else
    {
      /*#include "parallax-lit.inc"*/
      #include "parallax-lit-phase1.inc"
    }
}

static float phase = 0;
static float phase2 = 0;

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
parallax_mapping_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  scene_update_camera (&scene);

  tunnel_lighting ();
  light_update (scene.camera, &light0);
  update_lighting_texture (&scene, lighting_texture);
}

static void
draw_flat_texture (void)
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

static void
parallax_phase2 (void)
{
#include "parallax-lit-phase2.inc"
}

static void
parallax_phase3 (void)
{
#include "parallax-lit-phase3.inc"
}

static void
parallax_mapping_display_effect (uint32_t time_offset, void *params, int iparam,
				 GXRModeObj *rmode)
{
  GXTexObj stone_tex_obj, stone_depth_obj, height_bump_obj;
  GXTexObj texcoord_map_obj, texcoord_map2_obj;
  Mtx modelview, rot;
  Mtx scale;
  object_loc map_flat_loc;

  TPL_GetTexture (&stone_textureTPL, stone_texture, &stone_tex_obj);
  TPL_GetTexture (&stone_depthTPL, stone_depth, &stone_depth_obj);
  TPL_GetTexture (&height_bumpTPL, height_bump, &height_bump_obj);
  
  //GX_InitTexObjMaxAniso (&stone_tex_obj, GX_ANISO_4);
  
  GX_InitTexObjWrapMode (&stone_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjWrapMode (&stone_depth_obj, GX_CLAMP, GX_CLAMP);
  
  GX_LoadTexObj (&stone_tex_obj, GX_TEXMAP0);
  GX_LoadTexObj (&stone_depth_obj, GX_TEXMAP1);
#ifdef WITH_LIGHTING
  GX_LoadTexObj (&lighting_texture->texobj, GX_TEXMAP2);
  GX_LoadTexObj (&height_bump_obj, GX_TEXMAP3);
  GX_LoadTexObj (get_utility_texture (UTIL_TEX_16BIT_RAMP_NOREPEAT),
		 GX_TEXMAP4);
  GX_InitTexObj (&texcoord_map_obj, texcoord_map, TEXCOORD_MAP_W,
		 TEXCOORD_MAP_H, TEXCOORD_MAP_FMT, GX_CLAMP, GX_CLAMP,
		 GX_FALSE);
  GX_InitTexObjFilterMode (&texcoord_map_obj, GX_NEAR, GX_NEAR);
  GX_InitTexObj (&texcoord_map2_obj, texcoord_map, TEXCOORD_MAP_W,
		 TEXCOORD_MAP_H, TEXCOORD_MAP_FMT, GX_CLAMP, GX_CLAMP,
		 GX_FALSE);
  GX_InitTexObjFilterMode (&texcoord_map2_obj, GX_NEAR, GX_NEAR);
#endif

  rendertarget_texture (TEXCOORD_MAP_W, TEXCOORD_MAP_H, GX_CTF_GB8);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
      
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
#ifdef WITH_LIGHTING
#endif
  GX_SetCurrentMtx (GX_PNMTX0);
  
#ifdef WITH_LIGHTING
  texturing (1);
#else
  texturing (0);
#endif
#ifdef USE_GRID
  GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
# ifdef WITH_LIGHTING
  GX_SetIndTexCoordScale (GX_INDTEXSTAGE1, GX_ITS_1, GX_ITS_1);
# endif
#else
  GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_4, GX_ITS_4);
#endif
  {
    f32 indmtx[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
    /*float ang, dp;*/
#if 0
    guVector norm = { 0, 0, -1 };
    guVector eye = { 0, 0, -1 };
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
    scale = -3;
# else
    scale = -3;
# endif
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, scale);
# ifdef WITH_LIGHTING
    GX_SetIndTexMatrix (GX_ITM_1, indmtx, 4);
    /*GX_SetIndTexMatrix (GX_ITM_2, indmtx, -2);*/
# endif
#endif
  }
  
  GX_SetCullMode (GX_CULL_BACK);
  object_render (&plane_obj, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		 GX_VTXFMT0);

  GX_CopyTex (texcoord_map, GX_TRUE);
  GX_PixModeSync ();
  
  rendertarget_texture (TEXCOORD_MAP_W, TEXCOORD_MAP_H, GX_CTF_GB8);

  GX_LoadTexObj (&texcoord_map_obj, GX_TEXMAP5);

  parallax_phase2 ();
  draw_flat_texture ();
  
  GX_CopyTex (texcoord_map2, GX_TRUE);
  GX_PixModeSync ();
  
  rendertarget_screen (rmode);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_LoadTexObj (&texcoord_map2_obj, GX_TEXMAP6);

  {
    f32 indmtx[2][3] = { { 0.5, 0, 0 }, { 0, 0.5, 0 } };
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, 1);
    GX_SetIndTexMatrix (GX_ITM_1, indmtx, 2);
    GX_SetIndTexMatrix (GX_ITM_1, indmtx, -1);
  }
  /*draw_flat_texture ();*/
  
  object_loc_initialise (&map_flat_loc, GX_PNMTX0);
  object_set_screenspace_tex_matrix (&map_flat_loc, GX_TEXMTX4);
  object_set_tex_norm_binorm_matrices (&map_flat_loc, GX_TEXMTX2, GX_TEXMTX3);

  GX_SetTexCoordGen (GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX3);
  GX_SetTexCoordGen (GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX3);
  GX_SetTexCoordGen (GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD6, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX4);

  parallax_phase3 ();

  scene_update_matrices (&scene, &map_flat_loc, scene.camera, modelview, scale,
			 perspmat, GX_PERSPECTIVE);
  object_set_arrays (&plane_obj, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);
  object_render (&plane_obj, OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD,
		 GX_VTXFMT0);
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
