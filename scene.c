#include <assert.h>
#include <string.h>
#include <math.h>

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
		       Mtx obj_mtx, Mtx separate_scale, Mtx projection,
		       u32 projection_type)
{
  Mtx vertex, normal, binormal;
  Mtx normaltexmtx, binormaltexmtx, tempmtx;
  
  if (projection)
    GX_LoadProjectionMtx (projection, projection_type);
  
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
  
  /* A matrix which performs the same transformation as the camera & projection,
     but gives results in the range 0...1 suitable for use as texture
     coordinates.  */
  if (obj->calculate_screenspace_tex_mtx)
    {
      Mtx texproj;

      assert (projection);

      /* Get a matrix suitable for texture projection from the matrix used
         for scene projection.  */
      guMtxRowCol (texproj, 0, 0) = guMtxRowCol (projection, 0, 0) * 0.5;
      guMtxRowCol (texproj, 0, 1) = guMtxRowCol (projection, 0, 1);
      guMtxRowCol (texproj, 0, 2) = guMtxRowCol (projection, 0, 2) * 0.5 - 0.5;
      guMtxRowCol (texproj, 0, 3) = guMtxRowCol (projection, 0, 3);

      guMtxRowCol (texproj, 1, 0) = guMtxRowCol (projection, 1, 0);
      guMtxRowCol (texproj, 1, 1) = -guMtxRowCol (projection, 1, 1) * 0.5;
      guMtxRowCol (texproj, 1, 2) = guMtxRowCol (projection, 1, 2) * 0.5 - 0.5;
      guMtxRowCol (texproj, 1, 3) = guMtxRowCol (projection, 1, 3);

      guMtxRowCol (texproj, 2, 0) = guMtxRowCol (projection, 3, 0);
      guMtxRowCol (texproj, 2, 1) = guMtxRowCol (projection, 3, 1);
      guMtxRowCol (texproj, 2, 2) = guMtxRowCol (projection, 3, 2);
      guMtxRowCol (texproj, 2, 3) = guMtxRowCol (projection, 3, 3);

      guMtxConcat (texproj, vertex, tempmtx);
      
      GX_LoadTexMtxImm (tempmtx, obj->screenspace_tex_mtx, GX_MTX3x4);
    }

  if (obj->calculate_parallax_tex_mtx)
    {
      Mtx parmtx;
      static float q = 0, r = 0;
      
      guMtxCopy (vertex, parmtx);
      guMtxRowCol (parmtx, 0, 3) = 0.0;
      guMtxRowCol (parmtx, 1, 3) = 0.0;
      guMtxRowCol (parmtx, 2, 3) = 0.0;

      guMtxScale (tempmtx, 0.2, 0.2, 0.2);
      guMtxConcat (parmtx, tempmtx, parmtx);

      GX_LoadTexMtxImm (parmtx, obj->parallax_binorm_tex_mtx, GX_MTX2x4);
      
      //guMtxRowCol (parmtx, 0, 0) = sin (r);
      GX_LoadTexMtxImm (parmtx, obj->parallax_tangent_tex_mtx, GX_MTX2x4);
      
      q += 0.01;
      r += 0.012;
    }

  /* Calculate matrices used for texture coordinate generation for casting
     shadows (self-shadowing objects, at present).  */
  if (obj->calculate_shadowing_tex_mtx)
    {
      Mtx scaled_objmtx;
      
      guMtxConcat (obj_mtx, separate_scale, scaled_objmtx);
      
      /* Depth-ramp lookup.  */
      guMtxConcat (obj->shadow.info->shadow_tex_depth, scaled_objmtx, tempmtx);
      GX_LoadTexMtxImm (tempmtx, obj->shadow.ramp_tex_mtx, GX_MTX3x4);
      /* Shadow buffer lookup.  */
      guMtxConcat (obj->shadow.info->shadow_tex_projection, scaled_objmtx,
		   tempmtx);
      GX_LoadTexMtxImm (tempmtx, obj->shadow.buf_tex_mtx, GX_MTX3x4);
    }

  if (separate_scale)
    {
      guMtxConcat (obj_mtx, separate_scale, vertex);
      guMtxConcat (cam_mtx, vertex, vertex);
    }

  GX_LoadPosMtxImm (vertex, obj->pnmtx);
  GX_LoadNrmMtxImm (normal, obj->pnmtx);
}
