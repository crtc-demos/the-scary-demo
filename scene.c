#include "scene.h"

void
scene_set_pos (scene_info *scene, guVector pos)
{
  scene->pos = pos;
  scene->camera_dirty = 1;
}

void
scene_set_lookat (scene_info *scene, guVector lookat)
{
  scene->lookat = lookat;
  scene->camera_dirty = 1;
}

void
scene_set_up (scene_info *scene, guVector up)
{
  scene->up = up;
  scene->camera_dirty = 1;
}

void
scene_update_camera (scene_info *scene)
{
  if (scene->camera_dirty)
    {
      guLookAt (scene->camera, &scene->pos, &scene->up, &scene->lookat);
      scene->camera_dirty = 0;
    }
}

void
scene_update_matrices (scene_info *scene, object_loc *obj, Mtx cam_mtx,
		       Mtx obj_mtx)
{
  Mtx vertex, normal, binormal;
  Mtx normaltexmtx, binormaltexmtx, tempmtx;
  
  guMtxConcat (cam_mtx, obj_mtx, vertex);
  guMtxInverse (vertex, tempmtx);
  guMtxTranspose (tempmtx, normal);
  
  if (obj->calculate_normal_tex_mtx)
    {
      guMtxTrans (tempmtx, 0.5, 0.5, 0.0);
      guMtxConcat (tempmtx, normal, normaltexmtx);
      guMtxScale (tempmtx, NRM_SCALE / 2, NRM_SCALE / 2, NRM_SCALE / 2);
      guMtxConcat (normaltexmtx, tempmtx, normaltexmtx);

      GX_LoadTexMtxImm (normaltexmtx, obj->normal_tex_mtx, GX_MTX2x4);
    }
  
  if (obj->calculate_binorm_tex_mtx)
    {
      guMtxCopy (vertex, binormal);
      guMtxRowCol (binormal, 0, 3) = 0.0;
      guMtxRowCol (binormal, 1, 3) = 0.0;
      guMtxRowCol (binormal, 2, 3) = 0.0;

      guMtxScale (tempmtx, NRM_SCALE / 2, NRM_SCALE / 2, NRM_SCALE / 2);
      guMtxConcat (binormal, tempmtx, binormaltexmtx);

      GX_LoadTexMtxImm (binormaltexmtx, obj->binorm_tex_mtx, GX_MTX2x4);
    }
  
  if (obj->calculate_vertex_depth_mtx)
    {
      guMtxConcat (scene->depth_ramp_lookup, vertex, tempmtx);
      GX_LoadTexMtxImm (tempmtx, obj->vertex_depth_mtx, GX_MTX3x4);
    }

  GX_LoadPosMtxImm (vertex, obj->pnmtx);
  GX_LoadNrmMtxImm (normal, obj->pnmtx);
}
