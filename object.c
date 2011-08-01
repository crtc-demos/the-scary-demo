#include <ogcsys.h>
#include <gccore.h>
#include <stdlib.h>

#include "object.h"

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
