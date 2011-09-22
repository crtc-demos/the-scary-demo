#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "i8-tex-test.h"

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

static void
i8_tex_test_init_effect (void *params)
{
  TPL_OpenTPLFromMemory (&mighty_zebuTPL, (void *) mighty_zebu_tpl,
			 mighty_zebu_tpl_size);
  
  TPL_OpenTPLFromMemory (&spiderwebTPL, (void *) spiderweb_tpl,
			 spiderweb_tpl_size);
  
  grabbed_texture = memalign (32, GX_GetTexBufferSize (RTT_WIDTH, RTT_HEIGHT,
			      USEFMT, GX_FALSE, 0));
}

static void
i8_tex_test_uninit_effect (void *params)
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
    }

  GX_SetCullMode (GX_CULL_NONE);

  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT1, 4);

  GX_Position3f32 (3, -1, 1);
  GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (3, -1, -1);
  GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (3, 1, 1);
  GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (3, 1, -1);
  GX_TexCoord2f32 (0, 1);

  GX_End ();
}

static int thr = 0;

static void
i8_tex_test_display_effect (uint32_t time_offset, void *params, int iparam,
			    GXRModeObj *rmode)
{
  GXTexObj mighty_zebu_tex_obj;
  GXTexObj grabbed_tex_obj;
  GXTexObj spiderweb_tex_obj;
  
  TPL_GetTexture (&mighty_zebuTPL, mighty_zebu, &mighty_zebu_tex_obj);
  TPL_GetTexture (&spiderwebTPL, spiderweb, &spiderweb_tex_obj);

  /*GX_InitTexObjWrapMode (&mighty_zebu_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&mighty_zebu_tex_obj, GX_NEAR, GX_NEAR);*/

  GX_InitTexObj (&grabbed_tex_obj, grabbed_texture, RTT_WIDTH, RTT_HEIGHT,
		 USEFMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  
  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&mighty_zebu_tex_obj, GX_TEXMAP0);
  
  rendertarget_texture (RTT_WIDTH, RTT_HEIGHT, COPYFMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  draw_square (0, 0, 0);
  
  GX_CopyTex (grabbed_texture, GX_TRUE);
  
  GX_PixModeSync ();
  
  rendertarget_screen (rmode);
  
  GX_LoadTexObj (&grabbed_tex_obj, GX_TEXMAP0);
  
  draw_square (0, 0, 0);
  
  GX_LoadTexObj (&spiderweb_tex_obj, GX_TEXMAP1);
  
  draw_square (1, 128 + 127 * sin (thr * M_PI / 360.0),
	       128 + 127 * sin (thr * M_PI / 200.0));
  
  thr += 1;
}

effect_methods i8_tex_methods =
{
  .preinit_assets = NULL,
  .init_effect = &i8_tex_test_init_effect,
  .prepare_frame = NULL,
  .display_effect = &i8_tex_test_display_effect,
  .uninit_effect = &i8_tex_test_uninit_effect,
  .finalize = NULL
};
