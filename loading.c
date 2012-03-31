#include <ogcsys.h>
#include <gccore.h>

#include "object.h"
#include "shader.h"
#include "scene.h"

#include "images/loading.h"
#include "loading_tpl.h"

static TPLFile loadingTPL;

static GXTexObj loadimg_texobj;
static shader_info *loadimg_shader;

static void
loadimg_shader_setup (void *dummy)
{
#include "plain-texturing.inc"
}

static void
draw_tex_rect (shader_info *shader, u32 vtxfmt)
{
  Mtx mvtmp;
  object_loc rect_loc;
  scene_info rect_scene;
  Mtx44 ortho;
  const float xsize = 0.8, ysize = 0.37083333;

  /* "Straight" camera.  */
  scene_set_pos (&rect_scene, (guVector) { 0, 0, 0 });
  scene_set_lookat (&rect_scene, (guVector) { 5, 0, 0 });
  scene_set_up (&rect_scene, (guVector) { 0, 1, 0 });
  scene_update_camera (&rect_scene);

  guOrtho (ortho, -1, 1, -1, 1, 1, 15);
  
  object_loc_initialise (&rect_loc, GX_PNMTX0);
  
  guMtxIdentity (mvtmp);

  object_set_matrices (&rect_scene, &rect_loc, rect_scene.camera, mvtmp,
		       NULL, ortho, GX_ORTHOGRAPHIC);

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

  GX_SetVtxAttrFmt (vtxfmt, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  shader_load (shader);

  GX_SetCullMode (GX_CULL_NONE);

  GX_Begin (GX_TRIANGLESTRIP, vtxfmt, 4);

  GX_Position3f32 (3, -ysize, xsize);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (3, -ysize, -xsize);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (3, ysize, xsize);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (3, ysize, -xsize);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (0, 1);

  GX_End ();
}

void
draw_loading_screen (void)
{
  TPL_OpenTPLFromMemory (&loadingTPL, (void *) loading_tpl, loading_tpl_size);
  
  TPL_GetTexture (&loadingTPL, loading_image, &loadimg_texobj);
  
  loadimg_shader = create_shader (&loadimg_shader_setup, NULL);
  shader_append_texmap (loadimg_shader, &loadimg_texobj, GX_TEXMAP0);
  shader_append_texcoordgen (loadimg_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);

  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);
		     
  draw_tex_rect (loadimg_shader, GX_VTXFMT0);

  free_shader (loadimg_shader);
}

