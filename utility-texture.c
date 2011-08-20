/* Handling for "utility" textures -- automatically generated, sharable between
   different things.  E.g. ramp textures.  */

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#include "utility-texture.h"

static utility_texture_info util_textures[UTIL_TEX_MAX] = { { 0 } };

static void
create_8bit_ramp (utility_texture_info *dst)
{
  char *ramptex;
  unsigned int i, ramptexsize;
  
  dst->format = GX_TF_I8;
  
  ramptexsize = GX_GetTexBufferSize (16, 16, dst->format, GX_FALSE, 0);
  
  ramptex = memalign (32, ramptexsize);

  /* We are creating an 8-bit per texel ramp texture.  Each 32-byte cache
     line corresponds to an 8x4 (SxT) block of texels.  So we have:

     00 01 02 03 04 05 06 07 20 21 ... --> +S
     08 09 0a 0b 0c 0d 0e 0f 28 ...
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

  DCFlushRange (ramptex, ramptexsize);

  GX_InitTexObj (&dst->texobj, ramptex, 16, 16, dst->format, GX_CLAMP,
		 GX_REPEAT, GX_FALSE);
  GX_InitTexObjLOD (&dst->texobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0,
		    GX_ANISO_1);

  dst->texbuf = ramptex;
}

static void
create_16bit_ramp (utility_texture_info *dst)
{
  char *ramptex;
  unsigned int ramptexsize;
  unsigned int t_blk, s_blk, ctr = 0;

  dst->format = GX_TF_IA8;
  
  ramptexsize = GX_GetTexBufferSize (256, 256, dst->format, GX_FALSE, 0);
  
  ramptex = memalign (32, ramptexsize);

  /* Here we are generating 16-bit texels in IA8 format.  For each
     16-bit chunk the first byte is alpha, the second byte intensity.
     These texels form 4x4 (SxT) blocks of texels.  So we have (16-bit
     entries):

     00 02 04 06 20 22 24 26
     08 0a 0c 0e 28 2a 2c 2e
     10 12 14 16 38 ....
     18 1a 1c 1e
     ...  */

  /* This is definitely wrong!  */

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

  DCFlushRange (ramptex, ramptexsize);

  GX_InitTexObj (&dst->texobj, ramptex, 256, 256, GX_TF_IA8, GX_CLAMP,
		 GX_REPEAT, GX_FALSE);
  GX_InitTexObjLOD (&dst->texobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0, GX_ANISO_1);
  
  dst->texbuf = ramptex;
}

GXTexObj *
get_utility_texture (utility_texture_handle which)
{
  if (util_textures[which].texbuf == NULL)
    switch (which)
      {
      case UTIL_TEX_8BIT_RAMP:
	create_8bit_ramp (&util_textures[which]);
	break;

      case UTIL_TEX_16BIT_RAMP:
	create_16bit_ramp (&util_textures[which]);
	break;

      default:
	abort ();
      }
  
  return &util_textures[which].texobj;
}

void
free_utility_texture (utility_texture_handle which)
{
  if (util_textures[which].texbuf != NULL)
    {
      free (util_textures[which].texbuf);
      util_textures[which].texbuf = NULL;
    }
}
