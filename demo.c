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
#include <math.h>

#include "server.h"

#define DEFAULT_FIFO_SIZE	(512*1024)

static void *xfb = NULL;
static u32 do_copy = GX_FALSE;
static GXRModeObj *rmode = NULL;

#define MAJOR_STEPS 128
#define MINOR_STEPS 64

f32 *torus; // [MAJOR_STEPS * MINOR_STEPS * 3] ATTRIBUTE_ALIGN(32);

f32 *tornorms; // [MAJOR_STEPS * MINOR_STEPS * 3] ATTRIBUTE_ALIGN(32);

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
fill_torus_coords (float outer, float inner)
{
  int major;
  float major_ang = 0.0, major_incr = 2.0 * M_PI / MAJOR_STEPS;
  
  torus = memalign (32, MAJOR_STEPS * MINOR_STEPS * sizeof (f32) * 3);
  tornorms = memalign (32, MAJOR_STEPS * MINOR_STEPS * sizeof (f32) * 3);
  
  if (!torus)
    exit (0);
  
  if (!tornorms)
    exit (0);
  
  /*torus = MEM_K0_TO_K1 (torus);
  tornorms = MEM_K0_TO_K1 (tornorms);*/
  
  for (major = 0; major < MAJOR_STEPS; major++, major_ang += major_incr)
    {
      int minor;
      float fsin_major = sin (major_ang);
      float fcos_major = cos (major_ang);
      float maj_x = fcos_major * outer;
      float maj_y = fsin_major * outer;
      float minor_ang = 0.0, minor_incr = 2.0 * M_PI / MINOR_STEPS;
      
      for (minor = 0; minor < MINOR_STEPS; minor++, minor_ang += minor_incr)
        {
	  float fsin_minor = sin (minor_ang);
	  float fcos_minor = cos (minor_ang);
	  int idx = (major * MINOR_STEPS + minor) * 3;
	  float orig_min_x;
	  
	  tornorms[idx] = fcos_major * fsin_minor;
	  tornorms[idx + 1] = fsin_major * fsin_minor;
	  tornorms[idx + 2] = fcos_minor;
	  	  
	  orig_min_x = fsin_minor * inner;
	  torus[idx] = fcos_major * orig_min_x + maj_x;
	  torus[idx + 1] = fsin_major * orig_min_x + maj_y;
	  torus[idx + 2] = fcos_minor * inner;
	}
    }
  
  DCFlushRange (torus, MAJOR_STEPS * MINOR_STEPS * sizeof (f32) * 3);
  DCFlushRange (tornorms, MAJOR_STEPS * MINOR_STEPS * sizeof (f32) * 3);
}

static void
copy_to_xfb (u32 count)
{
  if (do_copy == GX_TRUE)
    {
      GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
      GX_SetColorUpdate (GX_TRUE);
      GX_SetAlphaUpdate (GX_TRUE);
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
plain_lighting (void)
{
  GXLightObj lo;

  GX_SetNumTexGens (0);
  GX_SetNumChans (1);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
  GX_SetNumTevStages (1);

  GX_SetChanAmbColor (GX_COLOR0A0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPOT);

  GX_InitLightPos (&lo, 20, 20, 30);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  // Initialise "a" parameters.
  GX_InitLightSpot (&lo, 0.0, GX_SP_OFF);
  // Initialise "k" parameters.
  GX_InitLightDistAttn (&lo, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightDir (&lo, 0.0, 0.0, 0.0);
  GX_LoadLightObj (&lo, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

/* Specular lighting needs two (channels?), apparently.  */

static void
specular_lighting (void)
{
  GXLightObj lo;
  guVector lpos = { 20, 20, 30 };

  GX_SetNumTexGens (0);
  GX_SetNumChans (2);
  GX_SetNumTevStages (2);

  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);

  GX_SetTevOrder (GX_TEVSTAGE1, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR1A1);

  GX_SetTevColorOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		    GX_ENABLE, GX_TEVPREV);
  GX_SetTevColorIn (GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_RASC, GX_CC_ONE,
		    GX_CC_CPREV);

  GX_SetTevAlphaOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		    GX_ENABLE, GX_TEVPREV);
  GX_SetTevAlphaIn (GX_TEVSTAGE1, GX_CA_APREV, GX_CA_ZERO, GX_CA_APREV,
		    GX_CA_ZERO);

  GX_SetChanAmbColor (GX_COLOR0A0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPOT);

  GX_SetChanAmbColor (GX_COLOR1A1, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_COLOR1A1, (GXColor) { 255, 255, 255, 0 });
  GX_SetChanCtrl (GX_COLOR1, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT1,
		  GX_DF_CLAMP, GX_AF_SPEC);


  /* Light 0: use for diffuse.  */
  GX_InitLightPos (&lo, lpos.x, lpos.y, lpos.z);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  // Initialise "a" parameters.
  GX_InitLightSpot (&lo, 0.0, GX_SP_OFF);
  // Initialise "k" parameters.
  GX_InitLightDistAttn (&lo, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightDir (&lo, 0.0, 0.0, 0.0);
  GX_LoadLightObj (&lo, GX_LIGHT0);

  guVecNormalize (&lpos);

  /* Light 1: use for specular.  Should be able to use the same light for
     both!  I think.  */
  GX_InitSpecularDir (&lo, -lpos.x, -lpos.y, -lpos.z);
  GX_InitLightShininess (&lo, 64);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  GX_LoadLightObj (&lo, GX_LIGHT1);

  GX_InvalidateTexAll ();
}

static void
draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX16);
  GX_SetVtxDesc (GX_VA_NRM, GX_INDEX16);
  //GX_SetVtxDesc (GX_VA_CLR0, GX_INDEX8);
  
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
  //GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  
  GX_SetArray (GX_VA_POS, torus, 3 * sizeof (f32));
  GX_SetArray (GX_VA_NRM, tornorms, 3 * sizeof (f32));
  //GX_SetArray (GX_VA_CLR0, colors, 4 * sizeof (u8));
  
  specular_lighting ();
}

static void
update_screen (Mtx viewMatrix)
{
  Mtx modelView, mvi, mvitmp;
  guVector axis = {0, 1, 0};
  static float deg = 0;
  int major, minor;

  guMtxIdentity (modelView);
  guMtxRotAxisDeg (modelView, &axis, deg);
  
  deg++;
  
  guMtxTransApply (modelView, modelView, 0.0F, 0.0F, -50.0F);
  guMtxConcat (viewMatrix, modelView, modelView);

  GX_LoadPosMtxImm (modelView, GX_PNMTX0);

  guMtxInverse (modelView, mvitmp);
  guMtxTranspose (mvitmp, mvi);
  GX_LoadNrmMtxImm (modelView, GX_PNMTX0);

  for (major = 0; major < MAJOR_STEPS; major++)
    {
      int mjidx = major * MINOR_STEPS;
      int mjidx1 = (major == MAJOR_STEPS - 1) ? 0
		  : (major + 1) * MINOR_STEPS;
      
      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, MINOR_STEPS * 2 + 2);

      GX_Position1x16 (mjidx1);
      GX_Normal1x16 (mjidx1);
      //GX_Color1x8 (0);
      GX_Position1x16 (mjidx);
      GX_Normal1x16 (mjidx);
      //GX_Color1x8 (0);

      for (minor = 1; minor <= MINOR_STEPS; minor++)
        {
	  int mnidx = (minor == MINOR_STEPS) ? 0 : minor;

	  GX_Position1x16 (mjidx1 + mnidx);
	  GX_Normal1x16 (mjidx1 + mnidx);
	  //GX_Color1x8 (0);
	  GX_Position1x16 (mjidx + mnidx);
	  GX_Normal1x16 (mjidx + mnidx);
	  //GX_Color1x8 (0);
	}
      GX_End();
    }
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
  int light_model = 0, switching_model = 0;

  xfb = initialise ();

  /* Hopefully this will trigger if we exit unexpectedly...  */
  atexit (return_to_loader);

  fill_torus_coords (10.0, 8.0);

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

      if (buttonsDown & PAD_BUTTON_A)
	switching_model = 1;
      else if (switching_model)
        {
	  switch (light_model)
	    {
	    case 0:
	      plain_lighting ();
	      break;
	    
	    case 1:
	      specular_lighting ();
	      break;
	    }

	  light_model = !light_model;
	  switching_model = 0;
	}
    }

  return 0;
}
