#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "rendertarget.h"
#include "scene.h"
#include "lighting-texture.h"

#define LIGHT_TEXFMT GX_TF_RGBA8
#define LIGHT_TEX_W 64
#define LIGHT_TEX_H 64

static Mtx44 proj;

lighting_texture_info *
create_lighting_texture (void)
{
  unsigned int texbufsize;
  lighting_texture_info *lt = calloc (1, sizeof (lighting_texture_info));

  texbufsize = GX_GetTexBufferSize (LIGHT_TEX_W, LIGHT_TEX_H, LIGHT_TEXFMT,
				    GX_FALSE, 0);
  lt->texbuf = memalign (32, texbufsize);
  
  GX_InitTexObj (&lt->texobj, lt->texbuf, LIGHT_TEX_W, LIGHT_TEX_H,
		 LIGHT_TEXFMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_InitTexObjFilterMode (&lt->texobj, GX_LINEAR, GX_LINEAR);
  
  return lt;
}

void
free_lighting_texture (lighting_texture_info *lt)
{
  free (lt->texbuf);
  free (lt);
}

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
	  c.x = (2.0 * (float) i / 32.0) - 1.0;
	  c.y = (2.0 * (float) j / 32.0) - 1.0;
	  c.z = -2.0;
	  cn.x = c.x / NRM_SCALE;
	  cn.y = c.y / NRM_SCALE;
	  cn.z = c.x * c.x + c.y * c.y;
	  cn.z = cn.z < 1 ? sqrtf (1.0 - cn.z) : 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	  
	  c.x = (2.0 * (float) (i + 1) / 32.0) - 1.0;
	  cn.x = c.x / NRM_SCALE;
	  cn.z = c.x * c.x + c.y * c.y;
	  cn.z = cn.z < 1 ? sqrtf (1.0 - cn.z) : 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	}
      
      GX_End ();
    }
}

void
update_lighting_texture (scene_info *scene, lighting_texture_info *lt)
{
  Mtx idmtx;
  object_loc obj_loc;
  
  object_loc_initialise (&obj_loc, GX_PNMTX0);

  rendertarget_texture (LIGHT_TEX_W, LIGHT_TEX_H, LIGHT_TEXFMT);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  guOrtho (proj, -1, 1, -1, 1, 1, 15);

  GX_SetPixelFmt (GX_PF_RGBA6_Z24, GX_ZC_LINEAR);

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);

  guMtxIdentity (idmtx);
  scene_update_matrices (scene, &obj_loc, idmtx, idmtx, NULL, proj,
			 GX_ORTHOGRAPHIC);

  hemisphere_texture ();

  GX_CopyTex (lt->texbuf, GX_TRUE);
  GX_PixModeSync ();
}
