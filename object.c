#include <ogcsys.h>
#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "object.h"
#include "scene.h"

void
object_loc_initialise (object_loc *loc, u32 pnmtx)
{
  memset (loc, 0, sizeof (object_loc));
  loc->pnmtx = pnmtx;
}

void
object_set_tex_norm_binorm_matrices (object_loc *obj, u32 normal_tex_mtx,
				     u32 binorm_tex_mtx)
{
  obj->calculate_normal_tex_mtx = 1;
  obj->normal_tex_mtx = normal_tex_mtx;
  obj->calculate_binorm_tex_mtx = 1;
  obj->binorm_tex_mtx = binorm_tex_mtx;
}

void
object_unset_tex_norm_binorm_matrices (object_loc *obj)
{
  obj->calculate_normal_tex_mtx = 0;
  obj->calculate_binorm_tex_mtx = 0;
}

void
object_set_tex_norm_matrix (object_loc *loc, u32 normal_tex_mtx)
{
  loc->calculate_normal_tex_mtx = 1;
  loc->normal_tex_mtx = normal_tex_mtx;
}

void
object_unset_tex_norm_matrix (object_loc *loc)
{
  loc->calculate_normal_tex_mtx = 0;
}

void
object_set_sph_envmap_matrix (object_loc *loc, u32 envmap_tex_mtx)
{
  loc->calculate_sph_envmap_tex_mtx = 1;
  loc->sph_envmap_tex_mtx = envmap_tex_mtx;
}

void
object_unset_sph_envmap_matrix (object_loc *loc)
{
  loc->calculate_sph_envmap_tex_mtx = 0;
}

void
object_set_vertex_depth_matrix (object_loc *obj, u32 vertex_depth_mtx)
{
  obj->calculate_vertex_depth_mtx = 1;
  obj->vertex_depth_mtx = vertex_depth_mtx;
}

void
object_unset_vertex_depth_matrix (object_loc *obj)
{
  obj->calculate_vertex_depth_mtx = 0;
}

void
object_set_screenspace_tex_matrix (object_loc *obj, u32 screenspace_tex_mtx)
{
  obj->calculate_screenspace_tex_mtx = 1;
  obj->screenspace_tex_mtx = screenspace_tex_mtx;
}

void
object_unset_screenspace_tex_matrix (object_loc *obj)
{
  obj->calculate_screenspace_tex_mtx = 0;
}

void
object_set_parallax_tex_matrices (object_loc *loc, u32 binorm_tex_mtx,
				  u32 tangent_tex_mtx, u32 texture_edge_length)
{
  loc->calculate_parallax_tex_mtx = 1;
  loc->parallax.binorm_tex_mtx = binorm_tex_mtx;
  loc->parallax.tangent_tex_mtx = tangent_tex_mtx;
  loc->parallax.texture_edge = texture_edge_length;
}

void
object_unset_parallax_tex_matrices (object_loc *loc)
{
  loc->calculate_parallax_tex_mtx = 0;
}

void
object_set_shadow_tex_matrix (object_loc *loc, u32 shadow_buf_tex_mtx,
			      u32 shadow_ramp_tex_mtx, shadow_info *shinf)
{
  loc->calculate_shadowing_tex_mtx = 1;
  loc->shadow.buf_tex_mtx = shadow_buf_tex_mtx;
  loc->shadow.ramp_tex_mtx = shadow_ramp_tex_mtx;
  loc->shadow.info = shinf;
}

void
object_unset_shadow_tex_matrix (object_loc *loc)
{
  loc->calculate_shadowing_tex_mtx = 0;
}

void
object_set_pos_norm_matrix (object_loc *obj, u32 pnmtx)
{
  obj->pnmtx = pnmtx;
}

/* This function doesn't need the SCENE parameter for much: consider
   removing.  */

void
object_set_matrices (scene_info *scene, object_loc *obj, Mtx cam_mtx,
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

  if (obj->calculate_sph_envmap_tex_mtx)
    {
      guMtxTrans (tempmtx, 0.5, 0.5, 0.0);
      guMtxConcat (tempmtx, normal, normaltexmtx);
      /* This is a fudge factor to avoid the edges of the circle, which may not
         be fully covered.  Should probably depend on the size of the texture
	 and/or the number of subdivisions used to reduce the cubemap to the
	 spheremap.   */
      guMtxScale (tempmtx, 0.495, 0.495, 0.495);
      guMtxConcat (normaltexmtx, tempmtx, normaltexmtx);

      GX_LoadTexMtxImm (normaltexmtx, obj->sph_envmap_tex_mtx, GX_MTX2x4);
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
      Mtx texproj, vertex_scaled;

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


      if (separate_scale)
	{
	  guMtxConcat (vertex, separate_scale, vertex_scaled);
	  guMtxConcat (texproj, vertex_scaled, tempmtx);
	}
      else
        {
	  guMtxConcat (texproj, vertex, tempmtx);
	}

      GX_LoadTexMtxImm (tempmtx, obj->screenspace_tex_mtx, GX_MTX3x4);
    }

  if (obj->calculate_parallax_tex_mtx)
    {
      Mtx parmtx, offset;
      float scale = 256.0 / (float) obj->parallax.texture_edge;
      
      /***
       ( s )   [ 1 0 0 0 ] ( +x )
       ( t ) = [ 0 1 0 0 ] (  0 )
       ( u )   [ 0 0 1 0 ] (  0 )
       
       ( s' )   [ s 0 0 ] ( s )
       ( t' ) = [ t 0 0 ] ( t )
       ( u' )             ( u )
       
       [  0  0  1 ] [ v00 v01 v02 ]    [  v20  v21  v22 ]
       [  0  1  0 ] [ v10 v11 v12 ] -> [  v10  v11  v12 ]
       [ -1  0  0 ] [ v20 v21 v22 ]    [ -v00 -v01 -v02 ]
       
      ***/
      guMtxRowCol (parmtx, 0, 0) = scale * guMtxRowCol (vertex, 2, 0);
      guMtxRowCol (parmtx, 0, 1) = scale * guMtxRowCol (vertex, 2, 1);
      guMtxRowCol (parmtx, 0, 2) = scale * guMtxRowCol (vertex, 2, 2);
      guMtxRowCol (parmtx, 0, 3) = 0;

      guMtxRowCol (parmtx, 1, 0) = 0;
      guMtxRowCol (parmtx, 1, 1) = 0;
      guMtxRowCol (parmtx, 1, 2) = 0;
      guMtxRowCol (parmtx, 1, 3) = 0;

      /* I'm still uncertain whether we (a) can or (b) should divide by the
         Z component.  */
      guMtxRowCol (parmtx, 2, 0) = 0;
      guMtxRowCol (parmtx, 2, 1) = 0;
      guMtxRowCol (parmtx, 2, 2) = 0;
      guMtxRowCol (parmtx, 2, 3) = 0;

      guMtxTrans (tempmtx, 0.0, 0.0, 0.0);
      guMtxConcat (tempmtx, parmtx, offset);

      GX_LoadTexMtxImm (offset, obj->parallax.binorm_tex_mtx, GX_MTX2x4);
      
      /***
      
       [  1  0  0 ] [ v00 v01 v02 ]    [  v00  v01  v02 ]
       [  0  0  1 ] [ v10 v11 v12 ] -> [  v20  v21  v22 ]
       [  0 -1  0 ] [ v20 v21 v22 ]    [ -v10 -v11 -v12 ]
      
      ***/

      guMtxRowCol (parmtx, 0, 0) = 0;
      guMtxRowCol (parmtx, 0, 1) = 0;
      guMtxRowCol (parmtx, 0, 2) = 0;

      guMtxRowCol (parmtx, 1, 0) = scale * guMtxRowCol (vertex, 2, 0);
      guMtxRowCol (parmtx, 1, 1) = scale * guMtxRowCol (vertex, 2, 1);
      guMtxRowCol (parmtx, 1, 2) = scale * guMtxRowCol (vertex, 2, 2);

      guMtxConcat (tempmtx, parmtx, offset);

      GX_LoadTexMtxImm (offset, obj->parallax.tangent_tex_mtx, GX_MTX2x4);
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

/* Set vertex (position/normal) arrays for an object. WHICH_FMT should be
   GX_VTXFMT0 etc. and WHICH_TEXCOORD should be GX_VA_TEX0 etc.  */

void
object_set_arrays (object_info *obj, unsigned int mask, int which_fmt,
		   int which_texcoord)
{
  GX_ClearVtxDesc ();

  if ((mask & OBJECT_POS) != 0)
    {
      GX_SetVtxDesc (GX_VA_POS, GX_INDEX16);
      GX_SetVtxAttrFmt (which_fmt, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
      GX_SetArray (GX_VA_POS, obj->positions, 3 * sizeof (f32));
    }
  
  if ((mask & OBJECT_NORM) != 0)
    {
      GX_SetVtxDesc (GX_VA_NRM, GX_INDEX16);
      GX_SetVtxAttrFmt (which_fmt, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
      GX_SetArray (GX_VA_NRM, obj->normals, 3 * sizeof (f32));
    }
  else if ((mask & OBJECT_NBT3) != 0)
    {
      GX_SetVtxDesc (GX_VA_NBT, GX_INDEX16);
      GX_SetVtxAttrFmt (which_fmt, GX_VA_NBT, GX_NRM_NBT3, GX_F32, 0);
      GX_SetArray (GX_VA_NBT, obj->normals, 3 * sizeof (f32));
    }
  
  if ((mask & OBJECT_TEXCOORD) != 0)
    {
      GX_SetVtxDesc (which_texcoord, GX_INDEX16);
      GX_SetVtxAttrFmt (which_fmt, which_texcoord, GX_TEX_ST, GX_F32, 0);
      GX_SetArray (which_texcoord, obj->texcoords, 2 * sizeof (f32));
    }
}

void
object_render (object_info *obj, unsigned int mask, int which_fmt)
{
  unsigned int i;
  
  switch (mask)
    {
    case OBJECT_POS | OBJECT_NORM:
      for (i = 0; i < obj->num_strips; i++)
	{
	  unsigned int j;

	  GX_Begin (GX_TRIANGLESTRIP, which_fmt, obj->strip_lengths[i]);

	  for (j = 0; j < obj->strip_lengths[i]; j++)
            {
	      GX_Position1x16 (obj->obj_strips[i][j * 3]);
	      GX_Normal1x16 (obj->obj_strips[i][j * 3 + 1]);
	    }

	  GX_End ();
	}
      break;

    case OBJECT_POS | OBJECT_NORM | OBJECT_TEXCOORD:
      for (i = 0; i < obj->num_strips; i++)
	{
	  unsigned int j;

	  GX_Begin (GX_TRIANGLESTRIP, which_fmt, obj->strip_lengths[i]);

	  for (j = 0; j < obj->strip_lengths[i]; j++)
            {
	      GX_Position1x16 (obj->obj_strips[i][j * 3]);
	      GX_Normal1x16 (obj->obj_strips[i][j * 3 + 1]);
	      GX_TexCoord1x16 (obj->obj_strips[i][j * 3 + 2]);
	    }

	  GX_End ();
	}
      break;

    default:
      {
        int stride = (mask & OBJECT_NBT3) ? 5 : 3;
	int tex_offset = (mask & OBJECT_NBT3) ? 4 : 2;

	for (i = 0; i < obj->num_strips; i++)
	  {
	    unsigned int j;

	    /*if ((rand () & 127) < 64)
	      continue;*/

	    GX_Begin (GX_TRIANGLESTRIP, which_fmt, obj->strip_lengths[i]);

	    for (j = 0; j < obj->strip_lengths[i]; j++)
              {
		if ((mask & OBJECT_POS) != 0)
		  GX_Position1x16 (obj->obj_strips[i][j * stride]);
		if ((mask & OBJECT_NORM) != 0)
		  GX_Normal1x16 (obj->obj_strips[i][j * stride + 1]);
		else if ((mask & OBJECT_NBT3) != 0)
		  {
		    GX_Normal1x16 (obj->obj_strips[i][j * stride + 1]);
		    GX_Normal1x16 (obj->obj_strips[i][j * stride + 2]);
		    GX_Normal1x16 (obj->obj_strips[i][j * stride + 3]);
		  }
		if ((mask & OBJECT_TEXCOORD) != 0)
		  GX_TexCoord1x16 (obj->obj_strips[i][j * stride + tex_offset]);
	      }

	    GX_End ();
	  }
      }
    }
}
