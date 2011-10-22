#include <ogcsys.h>
#include <gccore.h>

#include "rendertarget.h"

static int rendering_to_screen = 0;

void
rendertarget_screen (GXRModeObj *rmode)
{
  if (!rendering_to_screen)
    {
      GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
      GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
      GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
      GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
      GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);

      GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, GX_TRUE,
			rmode->vfilter);

      GX_SetFieldMode (rmode->field_rendering,
		       ((rmode->viHeight == 2 * rmode->xfbHeight)
			 ? GX_ENABLE : GX_DISABLE));

      if (rmode->aa)
	GX_SetPixelFmt (GX_PF_RGB565_Z16, GX_ZC_LINEAR);
      else
	GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);

      rendering_to_screen = 1;
    }
}

void
rendertarget_texture (u32 width, u32 height, u32 texfmt)
{
  GX_SetViewport (0, 0, width, height, 0, 1);
  GX_SetScissor (0, 0, width, height);
  GX_SetDispCopySrc (0, 0, width, height);
  GX_SetDispCopyDst (width, height);
  GX_SetDispCopyYScale (1);
  GX_SetCopyFilter (GX_FALSE, NULL, GX_FALSE, NULL);
  GX_SetTexCopySrc (0, 0, width, height);
  GX_SetTexCopyDst (width, height, texfmt, GX_FALSE);
  
  GX_ClearBoundingBox ();
  
  rendering_to_screen = 0;
}

void
rendertarget_minimise_copy (void)
{
  u16 t, b, l, r;
  
  GX_ReadBoundingBox (&t, &b, &l, &r);
  
  /* Round up/down to multiple of 2.  */
  l &= ~1;
  t &= ~1;
  r = (r + 1) & ~1;
  b = (b + 1) & ~1;
  
  GX_SetTexCopySrc (l, t, r - l, b - t);
}
