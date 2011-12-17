#include <malloc.h>

#include "backbuffer.h"
#include "server.h"

void
backbuffer_init (backbuffer_info *buf, u16 width, u16 height, u8 texfmt,
		 u8 copyfmt)
{
  if (buf->initialized)
    {
      srv_printf ("back buffer already initialised!\n");
      return;
    }
  
  buf->pixels = memalign (32, GX_GetTexBufferSize (width, height, texfmt,
			  GX_FALSE, 0));
  GX_InitTexObj (&buf->texobj, buf->pixels, width, height, texfmt, GX_CLAMP,
		 GX_CLAMP, GX_FALSE);

  buf->width = width;
  buf->height = height;
  buf->texfmt = texfmt;
  buf->copyfmt = copyfmt;
  buf->initialized = 1;
}

void
backbuffer_uninit (backbuffer_info *buf)
{
  if (!buf->initialized)
    {
      srv_printf ("back buffer not initialized!\n");
      return;
    }
  
  free (buf->pixels);
  buf->initialized = 0;
}
