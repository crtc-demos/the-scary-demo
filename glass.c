#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "glass.h"
#include "ghost-obj.h"

/*
#include "images/primary.h"
#include "primary_tpl.h"

static TPLFile primaryTPL;
*/

#include "images/mighty_zebu.h"
#include "mighty_zebu_tpl.h"

static TPLFile mighty_zebuTPL;

#include "images/spiderweb.h"
#include "spiderweb_tpl.h"

static TPLFile spiderwebTPL;

#include "object.h"
#include "scene.h"
#include "rendertarget.h"

static void *grabbed_texture;

#define COPYFMT GX_TF_RGBA8
#define USEFMT GX_TF_RGBA8

#define RTT_WIDTH 640
#define RTT_HEIGHT 480

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
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static object_loc obj_loc;

static void
ghost_init_effect (void *params)
{
  guPerspective (perspmat, 60, 1.33f, 10.0f, 500.0f);
  scene_update_camera (&scene);
  
  object_loc_initialise (&obj_loc, GX_PNMTX0);

  TPL_OpenTPLFromMemory (&mighty_zebuTPL, (void *) mighty_zebu_tpl,
			 mighty_zebu_tpl_size);
  
  TPL_OpenTPLFromMemory (&spiderwebTPL, (void *) spiderweb_tpl,
			 spiderweb_tpl_size);
  
  grabbed_texture = memalign (32, GX_GetTexBufferSize (RTT_WIDTH, RTT_HEIGHT,
			      USEFMT, GX_FALSE, 0));
}

static void
ghost_uninit_effect (void *params)
{
  free (grabbed_texture);
}

static void
draw_square (int shader, int threshold, int threshold2)
{
  Mtx mvtmp;
  object_loc square_loc;
  scene_info square_scene;
  Mtx44 ortho;
  
  /* "Straight" camera.  */
  scene_set_pos (&square_scene, (guVector) { 0, 0, 0 });
  scene_set_lookat (&square_scene, (guVector) { 5, 0, 0 });
  scene_set_up (&square_scene, (guVector) { 0, 1, 0 });
  scene_update_camera (&square_scene);
  
  guOrtho (ortho, -1, 1, -1, 1, 1, 15);
  
  object_loc_initialise (&square_loc, GX_PNMTX0);
  
  guMtxIdentity (mvtmp);
  
  scene_update_matrices (&square_scene, &square_loc, square_scene.camera, mvtmp,
			 NULL, ortho, GX_ORTHOGRAPHIC);

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  switch (shader)
    {
    case 0:
      {
	#include "plain-texturing.inc"
      }
      break;

    case 1:
      {
        #include "alpha-test.inc"
      }
      break;
      
    case 2:
      {
        #include "do-refraction.inc"
	/* Urgh, we want the same texcoord for direct & indirect matrices, but
	   our textures are different sizes, so we can't.  */
	GX_SetVtxDesc (GX_VA_TEX1, GX_DIRECT);
	GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
        GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY);
      }
      break;
    }

  GX_SetCullMode (GX_CULL_NONE);

  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT1, 4);

  GX_Position3f32 (3, -1, 1);
  GX_TexCoord2f32 (1, 0);
  if (shader == 2)
    GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (3, -1, -1);
  GX_TexCoord2f32 (0, 0);
  if (shader == 2)
    GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (3, 1, 1);
  GX_TexCoord2f32 (1, 1);
  if (shader == 2)
    GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (3, 1, -1);
  GX_TexCoord2f32 (0, 1);
  if (shader == 2)
    GX_TexCoord2f32 (0, 1);

  GX_End ();
}

static void
refraction_setup (void)
{
  #include "refraction.inc"
}

static void
glass_postpass (void)
{
  GXLightObj lo0;
  guVector ldir;

  #include "glass-postpass.inc"

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 128);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);
}

static int thr = 0;

static void
ghost_display_effect (uint32_t time_offset, void *params, int iparam,
			    GXRModeObj *rmode)
{
  GXTexObj mighty_zebu_tex_obj;
  GXTexObj grabbed_tex_obj;
  GXTexObj spiderweb_tex_obj;
  Mtx mvtmp, rot, mvtmp2;
  Mtx sep_scale;

  GX_LoadTexObj (get_utility_texture (UTIL_TEX_REFRACT), GX_TEXMAP2);
  
  TPL_GetTexture (&mighty_zebuTPL, mighty_zebu, &mighty_zebu_tex_obj);
  TPL_GetTexture (&spiderwebTPL, spiderweb, &spiderweb_tex_obj);

  GX_InitTexObjWrapMode (&mighty_zebu_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&mighty_zebu_tex_obj, GX_LINEAR, GX_LINEAR);

  GX_InitTexObj (&grabbed_tex_obj, grabbed_texture, RTT_WIDTH, RTT_HEIGHT,
		 USEFMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  
  GX_InvalidateTexAll ();
  
  /*
  GX_LoadTexObj (&mighty_zebu_tex_obj, GX_TEXMAP0);
  
  rendertarget_texture (RTT_WIDTH, RTT_HEIGHT, COPYFMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  draw_square (0, 0, 0);
  
  GX_SetCopyClear ((GXColor) { 128, 128, 128, 0 }, 0x00ffffff);
  
  GX_CopyTex (grabbed_texture, GX_TRUE);
  
  GX_PixModeSync ();
  */

  rendertarget_texture (RTT_WIDTH, RTT_HEIGHT, COPYFMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_SetCopyClear ((GXColor) { 128, 128, 128, 0 }, 0x00ffffff);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  /* We need a grey background!  This isn't very efficient though.  */
  GX_CopyTex (grabbed_texture, GX_TRUE);

  GX_SetCopyClear ((GXColor) { 0, 0, 0, 0 }, 0x00ffffff);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  GX_SetCullMode (GX_CULL_NONE);

  guMtxIdentity (mvtmp);
  guMtxRotAxisDeg (rot, &((guVector) { 0, 1, 0 }), thr);
  guMtxConcat (rot, mvtmp, mvtmp);
  guMtxRotAxisDeg (rot, &((guVector) { 1, 0, 0 }), thr * 0.7);
  guMtxConcat (rot, mvtmp, mvtmp);
  
  object_set_tex_norm_matrix (&obj_loc, GX_TEXMTX0);
  
  guMtxScale (sep_scale, 6.0, 6.0, 6.0);
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp, sep_scale,
			 perspmat, GX_PERSPECTIVE);

  light_update (scene.camera, &light0);
  
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX0);
  
  refraction_setup ();
  
  object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, 13, 0, 0);
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp2, sep_scale,
			 NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, -13, 0, 0);
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp2, sep_scale,
			 NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  GX_CopyTex (grabbed_texture, GX_TRUE);
  
  GX_PixModeSync ();

  rendertarget_screen (rmode);
  
  {
    f32 indmtx[2][3];
    
    /* Channels for indirect lookup go:
	 R -> -
	 G -> U
	 B -> T
	 A -> S
       We have G/B channels, so map those to S/T.  */
    
    indmtx[0][0] = 0.0; indmtx[0][1] = 0.5; indmtx[0][2] = 0.0;
    indmtx[1][0] = 0.0; indmtx[1][1] = 0.0; indmtx[1][2] = 0.5;
    
    GX_SetIndTexMatrix (GX_ITM_0, indmtx, 2);
  }

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);
  GX_SetCullMode (GX_CULL_NONE);

  GX_LoadTexObj (&grabbed_tex_obj, GX_TEXMAP0);
  GX_LoadTexObj (&mighty_zebu_tex_obj, GX_TEXMAP1);
  
  draw_square (2, 0, 0);
  
  /*GX_LoadTexObj (&spiderweb_tex_obj, GX_TEXMAP1);
  
  draw_square (1, 128 + 127 * sin (thr * M_PI / 360.0),
	       128 + 127 * sin (thr * M_PI / 200.0));*/

  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp, sep_scale,
			 perspmat, GX_PERSPECTIVE);

  glass_postpass ();

  GX_LoadTexObj (get_utility_texture (UTIL_TEX_DARKENING), GX_TEXMAP3);
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX0);

  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_SRCALPHA, GX_LO_SET);

  object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, 13, 0, 0);
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp2, sep_scale,
			 NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  guMtxTransApply (mvtmp, mvtmp2, -13, 0, 0);
  scene_update_matrices (&scene, &obj_loc, scene.camera, mvtmp2, sep_scale,
			 NULL, 0);
  object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);

  thr += 1;
}

effect_methods glass_methods =
{
  .preinit_assets = NULL,
  .init_effect = &ghost_init_effect,
  .prepare_frame = NULL,
  .display_effect = &ghost_display_effect,
  .uninit_effect = &ghost_uninit_effect,
  .finalize = NULL
};
