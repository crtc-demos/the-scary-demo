#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <debug.h>
#include <errno.h>
#include <unistd.h>

#include "server.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

static void *xfb = NULL;
static u32 do_copy = GX_FALSE;
static GXRModeObj *rmode = NULL;

s16 square[] ATTRIBUTE_ALIGN(32) =
{
  // x y z
  -30, -30,  0, 	// 0
  -30,  30,  0, 	// 1
   30, -30,  0, 	// 2
   30,  30,  0, 	// 3
};

// color data
u8 colors[] ATTRIBUTE_ALIGN(32) = {
  // r, g, b, a
  0,   255,   0,   0,	// 0 purple
  240,   0,   0, 255,	// 1 red
  255, 180,   0, 255,	// 2 orange
  255, 255,   0, 255,	// 3 yellow
   10, 120,  40, 255,	// 4 green
    0,  20, 100, 255	// 5 blue
};

static void
copy_to_xfb (u32 count)
{
  if (do_copy == GX_TRUE)
    {
      GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetColorUpdate (GX_TRUE);
      GX_CopyDisp (xfb, GX_TRUE);
      GX_Flush ();
      do_copy = GX_FALSE;
    }
}

GXTexObj texObj;
TPLFile spriteTPL;

void *
initialise ()
{
  void *framebuffer;
  void *gp_fifo;
  GXColor background = {0, 0, 0, 0xff};

  VIDEO_Init();
  PAD_Init();

  rmode = VIDEO_GetPreferredMode (NULL);
  framebuffer = MEM_K0_TO_K1 (SYS_AllocateFramebuffer (rmode));
 /* console_init (framebuffer, 20, 20, rmode->fbWidth, rmode->xfbHeight,
		rmode->fbWidth * VI_DISPLAY_PIX_SZ);*/

  VIDEO_Configure (rmode);
  VIDEO_SetNextFramebuffer (framebuffer);
  VIDEO_SetPostRetraceCallback (copy_to_xfb);
  VIDEO_SetBlack (FALSE);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();

  if (rmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();

  gp_fifo = MEM_K0_TO_K1 (memalign (32, DEFAULT_FIFO_SIZE));
  memset (gp_fifo, 0, DEFAULT_FIFO_SIZE);
  
  GX_Init (gp_fifo, DEFAULT_FIFO_SIZE);
  GX_SetCopyClear (background, 0x00ffffff);

  GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
  GX_SetDispCopyYScale ((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
  GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopySrc (0, 0, rmode->fbWidth, rmode->efbHeight);
  GX_SetDispCopyDst (rmode->fbWidth, rmode->xfbHeight);
  
  GX_SetCopyFilter (rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
  
  GX_SetFieldMode (rmode->field_rendering,
		   ((rmode->viHeight == 2 * rmode->xfbHeight)
		     ? GX_ENABLE : GX_DISABLE));

  if (rmode->aa)
    GX_SetPixelFmt (GX_PF_RGB565_Z16, GX_ZC_LINEAR);
  else
    GX_SetPixelFmt (GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  GX_SetCullMode (GX_CULL_NONE);
  GX_CopyDisp (framebuffer, GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);

  return framebuffer;
}

static void
draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
  
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  
  GX_SetArray (GX_VA_POS, square, 3 * sizeof (s16));
  GX_SetArray (GX_VA_CLR0, colors, 4 * sizeof (u8));
  
  GX_SetNumTexGens (0);
  GX_SetNumChans (1);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);

  GX_InvalidateTexAll ();
}

static void
update_screen (Mtx viewMatrix)
{
  Mtx modelView;

  guMtxIdentity (modelView);
  guMtxTransApply (modelView, modelView, 0.0F, 0.0F, -50.0F);
  guMtxConcat(viewMatrix,modelView,modelView);

  GX_LoadPosMtxImm(modelView, GX_PNMTX0);

  GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

  GX_Position1x8(0);
  GX_Color1x8(0);
  GX_Position1x8(1);
  GX_Color1x8(1);
  GX_Position1x8(2);
  GX_Color1x8(2);
  GX_Position1x8(3);
  GX_Color1x8(3);

  GX_End();
}

static void
return_to_loader (void)
{
  void (*reload)() = (void(*)()) 0x80001800;
  srv_disconnect ();
  reload ();
}

int
main (int argc, char **argv)
{
  Mtx viewmat, perspmat;
  guVector pos = {0, 0, 0};
  guVector up = {0, 1, 0};
  guVector lookat = {0, 0, -1};
  s32 ret;
  char localip[16] = {0};
  char gateway[16] = {0};
  char netmask[16] = {0};

  xfb = initialise ();

  /* Hopefully this will trigger if we exit unexpectedly...  */
  atexit (return_to_loader);

  printf("Configuring network ...\n");

  strcpy (localip, "192.168.2.254");
  strcpy (gateway, "192.168.2.251");
  strcpy (netmask, "192.168.2.248");

  // Configure the network interface
  ret = if_config (localip, netmask, gateway, FALSE);

  if (ret >= 0)
    printf ("network configured, ip: %s, gw: %s, mask %s\n", localip,
	    gateway, netmask);
  else
    printf ("network configuration failed!\n");

  printf ("waiting for connection...\n");
  srv_wait_for_connection ();
  printf ("got connection!\n");

  srv_printf ("framebuffer = %p\n", xfb);

  guPerspective (perspmat, 60, 1.33f, 10.0f, 300.0f);
  GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);

  draw_init ();

  while (1)
    {
      guLookAt (viewmat, &pos, &up, &lookat);
      
      GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
      GX_InvVtxCache ();
      GX_InvalidateTexAll ();
      
      update_screen (viewmat);

      GX_DrawDone ();
      do_copy = GX_TRUE;
      VIDEO_WaitVSync();

      PAD_ScanPads();

      int buttonsDown = PAD_ButtonsDown (0);

      if (buttonsDown & PAD_BUTTON_START)
        return_to_loader ();
    }

  return 0;
}
