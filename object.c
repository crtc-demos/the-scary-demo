#include "object.h"

/* Set vertex (position/normal) arrays for an object. WHICH_FMT should be
   GX_VTXFMT0 etc.  */

void
object_set_arrays (object_info *obj, int which_fmt)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX16);
  GX_SetVtxDesc (GX_VA_NRM, GX_INDEX16);
  
  GX_SetVtxAttrFmt (which_fmt, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (which_fmt, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  
  GX_SetArray (GX_VA_POS, obj->positions, 3 * sizeof (f32));
  GX_SetArray (GX_VA_NRM, obj->normals, 3 * sizeof (f32));
}

void
object_render (Mtx viewMatrix, int do_texture_mats, u16 **obj_strips,
	       unsigned int *strip_lengths, unsigned int num_strips)
{
  Mtx modelView, mvi, mvitmp, scale, rotmtx;
  guVector axis = {0, 1, 0};
  guVector axis2 = {1, 0, 0};
  unsigned int i;
  
  guMtxIdentity (modelView);
  guMtxRotAxisDeg (modelView, &axis, deg);

  guMtxRotAxisDeg (rotmtx, &axis2, deg2);

  guMtxConcat (modelView, rotmtx, modelView);

#if 1
  /* empty */
#elif defined(SHIP)
  guMtxScale (scale, 3.0, 3.0, 3.0);
#elif defined(SWIRL)
  guMtxScale (scale, 15.0, 15.0, 15.0);
#elif defined(DUCK)
  guMtxScale (scale, 30.0, 30.0, 30.0);
#endif
  guMtxConcat (modelView, scale, modelView);

  if (do_texture_mats)
    {
      guMtxConcat (depth, modelView, mvitmp);
      GX_LoadTexMtxImm (mvitmp, GX_TEXMTX0, GX_MTX3x4);
      guMtxConcat (texproj, modelView, mvitmp);
      GX_LoadTexMtxImm (mvitmp, GX_TEXMTX1, GX_MTX3x4);
    }

  //guMtxTransApply (modelView, modelView, 0.0F, 0.0F, 0.0F);
  guMtxConcat (viewMatrix, modelView, modelView);

  GX_LoadPosMtxImm (modelView, GX_PNMTX0);

  guMtxInverse (modelView, mvitmp);
  guMtxTranspose (mvitmp, mvi);
  GX_LoadNrmMtxImm (mvi, GX_PNMTX0);
  
  for (i = 0; i < num_strips; i++)
    {
      unsigned int j;

      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, strip_lengths[i]);
      
      for (j = 0; j < strip_lengths[i]; j++)
        {
	  GX_Position1x16 (obj_strips[i][j * 3]);
	  GX_Normal1x16 (obj_strips[i][j * 3 + 1]);
	}
      
      GX_End ();
    }
}
