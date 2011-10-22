/* Handling for "utility" textures -- automatically generated, sharable between
   different things.  E.g. ramp textures.  */

#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

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

static unsigned int
tex_index (unsigned int s, unsigned int t, unsigned int pitch,
	   unsigned int texel_bits)
{
  unsigned int s_blk, t_blk, s_lo, t_lo;
  
  if (texel_bits == 8)
    {
      s_blk = s >> 3;
      t_blk = t >> 2;
      
      s_lo = s & 7;
      t_lo = t & 3;
      
      return s_lo + (t_lo * 8) + (s_blk * 32) + (t_blk * 4 * pitch);
    }
  else if (texel_bits == 16)
    {
      s_blk = s >> 2;
      t_blk = t >> 2;

      s_lo = s & 3;
      t_lo = t & 3;
      
      return (s_lo + (t_lo * 4) + (s_blk * 16) + (t_blk * 4 * pitch)) * 2;
    }

  return -1;
}

static void
create_16bit_ramp (utility_texture_info *dst, int norepeat)
{
  char *ramptex;
  unsigned int ramptexsize;
  unsigned int t, s;

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

  for (t = 0; t < 256; t++)
    for (s = 0; s < 256; s++)
      {
	unsigned int idx = tex_index (s, t, 256, 16);

	/* alpha->low byte, intensity->high byte.  */
	ramptex[idx] = s;
	ramptex[idx + 1] = t;
      }

  DCFlushRange (ramptex, ramptexsize);

  GX_InitTexObj (&dst->texobj, ramptex, 256, 256, dst->format, GX_CLAMP,
		 norepeat ? GX_CLAMP : GX_REPEAT, GX_FALSE);
  GX_InitTexObjLOD (&dst->texobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0, GX_ANISO_1);
  
  dst->texbuf = ramptex;
}

static void
create_refract (utility_texture_info *dst)
{
  char *refrtex;
  unsigned int refrtexsize, s, t;
  
  dst->format = GX_TF_IA8;
  
  refrtexsize = GX_GetTexBufferSize (128, 128, dst->format, GX_FALSE, 0);
  
  refrtex = memalign (32, refrtexsize);
  
  for (t = 0; t < 128; t++)
    {
      float tf = /*1.0 -*/ ((float) t / 127.0);

      for (s = 0; s < 128; s++)
	{
	  float sf = 1.0 - ((float) s / 127.0);
	  unsigned int idx;
	  
	  idx = tex_index (s, t, 128, 16);
	  refrtex[idx] = 255.0 * sf;
	  refrtex[idx + 1] = 255.0 * tf;
	}
    }
  
  DCFlushRange (refrtex, refrtexsize);
  
  GX_InitTexObj (&dst->texobj, refrtex, 128, 128, dst->format, GX_CLAMP,
		 GX_CLAMP, GX_FALSE);

  dst->texbuf = refrtex;
}

static void
create_darkening (utility_texture_info *dst)
{
  char *darkentex;
  unsigned int darkentexsize, s, t;
  
  dst->format = GX_TF_I8;
  
  darkentexsize = GX_GetTexBufferSize (128, 128, dst->format, GX_FALSE, 0);
  
  darkentex = memalign (32, darkentexsize);
  
  for (t = 0; t < 128; t++)
    {
      float tf = (t / 127.0) * 2.0 - 1.0;
      for (s = 0; s < 128; s++)
	{
	  float sf = (s / 127.0) * 2.0 - 1.0;
	  float rad = 1.0 - sf * sf - tf * tf;
	  unsigned int idx;
	  
	  /* This is just a value which happens to look OK, not physically
	     correct.  */
	  if (rad > 0)
	    rad = powf (rad, 0.3);
	  else
	    rad = 0;
	  
	  idx = tex_index (s, t, 128, 8);
	  darkentex[idx] = 255.0 * rad;
	}
    }
  
  DCFlushRange (darkentex, darkentexsize);
  
  GX_InitTexObj (&dst->texobj, darkentex, 128, 128, dst->format, GX_CLAMP,
		 GX_CLAMP, GX_FALSE);

  dst->texbuf = darkentex;
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
      case UTIL_TEX_16BIT_RAMP_NOREPEAT:
	create_16bit_ramp (&util_textures[which],
			   which == UTIL_TEX_16BIT_RAMP_NOREPEAT);
	break;

      case UTIL_TEX_REFRACT:
        create_refract (&util_textures[which]);
	break;

      case UTIL_TEX_DARKENING:
        create_darkening (&util_textures[which]);
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
