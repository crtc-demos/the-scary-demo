#include <ogcsys.h>
#include <gccore.h>

#include "object.h"
#include "scene.h"
#include "shader.h"
#include "screenspace.h"

void
screenspace_rect (shader_info *shader, u32 vtxfmt, int texcoords_twice)
{
  Mtx mvtmp;
  object_loc rect_loc;
  scene_info rect_scene;
  Mtx44 ortho;

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
  if (texcoords_twice)
    GX_SetVtxDesc (GX_VA_TEX1, GX_DIRECT);

  GX_SetVtxAttrFmt (vtxfmt, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  if (texcoords_twice)
    GX_SetVtxAttrFmt (vtxfmt, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);

  shader_load (shader);

  GX_SetCullMode (GX_CULL_NONE);

  GX_Begin (GX_TRIANGLESTRIP, vtxfmt, 4);

  GX_Position3f32 (3, -1, 1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (1, 0);
  if (texcoords_twice)
    GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (3, -1, -1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (0, 0);
  if (texcoords_twice)
    GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (3, 1, 1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (1, 1);
  if (texcoords_twice)
    GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (3, 1, -1);
  GX_Normal3f32 (-1, 0, 0);
  GX_TexCoord2f32 (0, 1);
  if (texcoords_twice)
    GX_TexCoord2f32 (0, 1);

  GX_End ();
}
