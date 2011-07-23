#include <ogcsys.h>
#include <gccore.h>

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
object_render (object_info *obj, int which_fmt)
{
  unsigned int i;
  
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
}
