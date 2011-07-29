#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "spooky-ghost.h"
#include "light.h"
#include "object.h"
#include "server.h"

#include "images/more_stones.h"
#include "more_stones_tpl.h"

static TPLFile stone_textureTPL;

#include "images/stones_bump.h"
#include "stones_bump_tpl.h"

static TPLFile stone_bumpTPL;

#include "objects/spooky-ghost.inc"
#include "objects/tunnel-section.inc"

INIT_OBJECT (spooky_ghost_obj, spooky_ghost);
INIT_OBJECT (tunnel_section_obj, tunnel_section);

#define NORM_TYPE OBJECT_NBT3

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 perspmat;
static Mtx viewmat;
static guVector pos = {0, 0, 30};
static guVector up = {0, 1, 0};
static guVector lookat = {0, 0, 0};
static float deg = 0.0;
static float deg2 = 0.0;

static void
specular_lighting_1light (void)
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
		  GX_DF_CLAMP, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

static void
bump_mapping_tev_setup (void)
{
  f32 indmtx[2][3] = { { 0.5, 0.0, 0.0 },
		       { 0.0, 0.5, 0.0 } };
#include "bump-mapping.inc"
  GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 10);
}

void *lightmap;

static void
spooky_ghost_init_effect (void *params)
{
  guPerspective (perspmat, 60, 1.33f, 10.0f, 300.0f);
  guLookAt (viewmat, &pos, &up, &lookat);
  
  TPL_OpenTPLFromMemory (&stone_textureTPL, (void *) more_stones_tpl,
			 more_stones_tpl_size);
  TPL_OpenTPLFromMemory (&stone_bumpTPL, (void *) stones_bump_tpl,
			 stones_bump_tpl_size);

  lightmap = memalign (32,
		       GX_GetTexBufferSize (64, 64, GX_TF_IA8, GX_FALSE, 0));
}

#define NRM_SCALE 0.8

static void
hemisphere_texture (void)
{
  u32 i, j;
  guVector c, cn;
  
  for (i = 0; i < 32; i++)
    {
      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, 32 * 2 + 2);
      
      for (j = 0; j <= 32; j++)
        {
	  c.x = 2.0 * (float) i / 32 - 1.0;
	  c.y = 2.0 * (float) j / 32 - 1.0;
	  c.z = -2.0;
	  cn.x = c.x / NRM_SCALE;
	  cn.y = c.y / NRM_SCALE;
	  cn.z = c.x * c.x + c.y * c.y;
	  cn.z = cn.z < 1 ? sqrtf (1.0 - cn.z) : 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	  
	  c.x = 2.0 * ((float) i + 1.0) / 32.0 - 1.0;
	  cn.x = c.x / NRM_SCALE;
	  cn.z = c.x * c.x + c.y * c.y;
	  cn.z = cn.z < 1 ? sqrtf (1.0 - cn.z) : 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	}
      
      GX_End ();
    }
}

static void
update_matrices (Mtx obj, Mtx cam)
{
  Mtx vertex, normal, binormal;
  Mtx normaltexmtx, binormaltexmtx, texturemtx, tempmtx;
  
  guMtxConcat (cam, obj, vertex);
  guMtxInverse (vertex, tempmtx);
  guMtxTranspose (tempmtx, normal);
  
  guMtxTrans (tempmtx, 0.5, 0.5, 0.0);
  guMtxConcat (tempmtx, normal, normaltexmtx);
  guMtxScale (tempmtx, NRM_SCALE / 2, NRM_SCALE / 2, NRM_SCALE / 2);
  guMtxConcat (normaltexmtx, tempmtx, normaltexmtx);
  
  guMtxCopy (vertex, binormal);
  guMtxRowCol (binormal, 0, 3) = 0.0;
  guMtxRowCol (binormal, 1, 3) = 0.0;
  guMtxRowCol (binormal, 2, 3) = 0.0;
  
  guMtxConcat (binormal, tempmtx, binormaltexmtx);

  guMtxIdentity (texturemtx);

  GX_LoadPosMtxImm (vertex, GX_PNMTX0);
  GX_LoadNrmMtxImm (normal, GX_PNMTX0);
  GX_LoadTexMtxImm (texturemtx, GX_TEXMTX0, GX_MTX2x4);
  GX_LoadTexMtxImm (normaltexmtx, GX_TEXMTX1, GX_MTX2x4);
  GX_LoadTexMtxImm (binormaltexmtx, GX_TEXMTX2, GX_MTX2x4);
  
  light_update (cam, &light0);
}

static void
spooky_ghost_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  Mtx44 proj;
  Mtx idmtx;

  GX_SetViewport (0, 0, 64, 64, 0, 1);
  GX_SetScissor (0, 0, 64, 64);
  GX_SetDispCopySrc (0, 0, 64, 64);
  GX_SetDispCopyDst (64, 64);
  GX_SetDispCopyYScale (1);
  GX_SetTexCopySrc (0, 0, 64, 64);
  GX_SetTexCopyDst (64, 64, GX_TF_IA8, FALSE);
  
  guOrtho (proj, -1, 1, -1, 1, 1, 15);
  GX_LoadProjectionMtx (proj, GX_ORTHOGRAPHIC);
  
  GX_SetPixelFmt (GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
  
  specular_lighting_1light ();

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);

  guMtxIdentity (idmtx);
  update_matrices (idmtx, idmtx);
  
  hemisphere_texture ();
  
  GX_CopyTex (lightmap, GX_FALSE);
  GX_PixModeSync ();
}

static void
spooky_ghost_display_effect (uint32_t time_offset, void *params, int iparam,
			     GXRModeObj *rmode)
{
  Mtx modelView, rotmtx, rotmtx2, mvtmp;
  guVector axis = {0, 1, 0};
  guVector axis2 = {0, 0, 1};
  GXTexObj texture;
  GXTexObj bumpmap;
  GXTexObj lightmap_obj;

  TPL_GetTexture (&stone_textureTPL, stone_texture, &texture);
  TPL_GetTexture (&stone_bumpTPL, stone_bump, &bumpmap);

  GX_InitTexObj (&lightmap_obj, lightmap, 64, 64, GX_TF_IA8, GX_CLAMP,
		 GX_CLAMP, GX_FALSE);

  GX_InvalidateTexAll ();
  
  GX_LoadTexObj (&texture, GX_TEXMAP0);
  GX_LoadTexObj (&lightmap_obj, GX_TEXMAP1);
  GX_LoadTexObj (&bumpmap, GX_TEXMAP2);

  GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
  GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
  
  GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
  
  GX_SetFieldMode (rmode->field_rendering,
		   ((rmode->viHeight == 2 * rmode->xfbHeight)
		     ? GX_ENABLE : GX_DISABLE));

  if (rmode->aa)
    GX_SetPixelFmt (GX_PF_RGB565_Z16, GX_ZC_LINEAR);
  else
    GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  bump_mapping_tev_setup ();
  
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX2);
  GX_SetTexCoordGen (GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX1);
  
  GX_SetCurrentMtx (GX_PNMTX0);
  
  GX_SetAlphaUpdate (GX_FALSE);
  
  object_set_arrays (&tunnel_section_obj,
		     OBJECT_POS | NORM_TYPE | OBJECT_TEXCOORD,
		     GX_VTXFMT0, GX_VA_TEX0);

  guMtxIdentity (modelView);
  guMtxRotAxisDeg (rotmtx, &axis, deg);
  guMtxRotAxisDeg (rotmtx2, &axis2, deg2);

  guMtxScaleApply (modelView, mvtmp, 20.0, 20.0, 20.0);

  guMtxConcat (rotmtx, mvtmp, mvtmp);
  guMtxConcat (rotmtx2, mvtmp, mvtmp);
  
  update_matrices (mvtmp, viewmat);
  
  object_render (&tunnel_section_obj,
		 OBJECT_POS | NORM_TYPE | OBJECT_TEXCOORD, GX_VTXFMT0);

  deg++;
  deg2 += 0.5;
}

effect_methods spooky_ghost_methods =
{
  .preinit_assets = NULL,
  .init_effect = &spooky_ghost_init_effect,
  .prepare_frame = &spooky_ghost_prepare_frame,
  .display_effect = &spooky_ghost_display_effect,
  .uninit_effect = NULL,
  .finalize = NULL
};
