#include <stdlib.h>

#include "shadow.h"

/* Make a ramp texture. ENTRIES can be 256 or 65536.  */

shadow_info *
create_shadow_info (int entries, light_info *light)
{
  shadow_info *shinf;
  unsigned int i;
  char *ramptex;
  unsigned int ramptexfmt;
  unsigned int ramptexsize;
  
  shinf = calloc (1, sizeof (shadow_info));
  
  switch (entries)
    {
    case 256:
      ramptexfmt = GX_TF_I8;
      ramptexsize = GX_GetTexBufferSize (16, 16, ramptexfmt, GX_FALSE, 0);
      break;

    case 65536:
      ramptexfmt = GX_TF_IA8;
      ramptexsize = GX_GetTexBufferSize (256, 256, ramptexfmt, GX_FALSE, 0);
      break;

    default:
      abort ();
    }
  
  ramptex = memalign (32, ramptexsize);

  switch (entries)
    {
    case 256:
      /* We are creating an 8-bit per texel ramp texture.  Each 32-byte cache
         line corresponds to an 8x4 (SxT) block of texels.  So we have:
	 
	 00 01 02 03 04 05 06 07 ... --> +S
	 08 09 0a 0b 0c 0d 0e 0f ...
	 10 11 12 13 14 15 16 17 ...
	 18 19 1a 1b 1c 1d 1e 1f ...
	  |  ...
	  V  ...
	 +T
      */
	 
      for (i = 0; i < 256; i++)
        {
	  unsigned int offset = ((i & 0x80) >> 2)
				+ ((i & 0x70) >> 4)
				+ ((i & 0x0c) << 4)
				+ ((i & 0x03) << 3);
	  ramptex[offset] = i;
	}
      
      GX_InitTexObj (&shinf->ramptexobj, ramptex, 16, 16, GX_TF_I8,
		     GX_CLAMP, GX_REPEAT, GX_FALSE);
      GX_InitTexObjLOD (&shinf->ramptexobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0,
			GX_ANISO_1);
      break;
    
    case 65536:
      {
        /* Here we are generating 16-bit texels in IA8 format.  For each
	   16-bit chunk the first byte is alpha, the second byte intensity.
	   These texels form 4x4 (SxT) blocks of texels.  So we have (16-bit
	   entries):
	   
	   00 01 02 03 10 11 12 13
	   04 05 06 07 14 15 16 17
	   08 09 0a 0b 18 ....
	   0c 0d 0e 0f
	   ...  */
        unsigned int t_blk, s_blk, ctr = 0;
	
	for (t_blk = 0; t_blk < 64; t_blk++)
	  for (s_blk = 0; s_blk < 64; s_blk++)
	    {
	      unsigned int s, t;
	      
	      for (t = 0; t < 4; t++)
	        for (s = 0; s < 4; s++)
		  {
		    unsigned int idx = (s_blk * 4 + s) + 256 * (t_blk * 4 + t);
		    /* alpha->low byte, intensity->high byte.  */
		    ramptex[idx * 2] = ctr & 255;
		    ramptex[idx * 2 + 1] = (ctr >> 8) & 255;
		    ctr++;
		  }
	    }

	assert (ctr == 65536);
	
	GX_InitTexObj (&shinf->ramptexobj, ramptex, 256, 256, GX_TF_IA8,
		       GX_CLAMP, GX_REPEAT, GX_FALSE);
	GX_InitTexObjLOD (&shinf->ramptexobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0,
			  GX_ANISO_1);
      break;
    
    default:
      abort ();
    }

  DCFlushRange (ramptex, ramptexsize);
  
  shinf->ramptexbuf = ramptex;
  shinf->light = light;

  return shinf;
}

void
destroy_shadow_info (shadow_info *shinf)
{
  free (shinf->ramptexbuf);
  free (shinf);
}
