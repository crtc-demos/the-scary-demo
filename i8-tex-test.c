#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "i8-tex-test.h"

#include "images/primary.h"
#include "primary_tpl.h"
#include "object.h"
#include "scene.h"
#include "rendertarget.h"

static TPLFile primaryTPL;

static void *grabbed_texture;

#define COPYFMT GX_CTF_RG8
#define USEFMT GX_TF_IA8

static void
i8_tex_test_init_effect (void *params)
{
  TPL_OpenTPLFromMemory (&primaryTPL, (void *) primary_tpl, primary_tpl_size);
  
  grabbed_texture = memalign (32, GX_GetTexBufferSize (256, 256, USEFMT,
			      GX_FALSE, 0));
}

static void
i8_tex_test_uninit_effect (void *params)
{
  free (grabbed_texture);
}

static void
draw_square (void)
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

#include "plain-texturing.inc"

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

static void
i8_tex_test_display_effect (uint32_t time_offset, void *params, int iparam,
			    GXRModeObj *rmode)
{
  GXTexObj primary_tex_obj;
  GXTexObj grabbed_tex_obj;
  
  TPL_GetTexture (&primaryTPL, primary, &primary_tex_obj);

  GX_InitTexObjWrapMode (&primary_tex_obj, GX_CLAMP, GX_CLAMP);
  GX_InitTexObjFilterMode (&primary_tex_obj, GX_NEAR, GX_NEAR);

  GX_InitTexObj (&grabbed_tex_obj, grabbed_texture, 256, 256, USEFMT,
		 GX_CLAMP, GX_CLAMP, GX_FALSE);
  
  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&primary_tex_obj, GX_TEXMAP0);
  
  rendertarget_texture (256, 256, COPYFMT);
  GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_FALSE);

  draw_square ();
  
  GX_CopyTex (grabbed_texture, GX_TRUE);
  
  GX_PixModeSync ();
  
  rendertarget_screen (rmode);
  
  GX_LoadTexObj (&grabbed_tex_obj, GX_TEXMAP0);
  
  draw_square ();
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
