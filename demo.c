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

#define SHIP
#undef SWIRL
#undef DUCK

#ifdef SHIP
#include "../objconvert/ship.inc"
#elif defined(SWIRL)
#include "../objconvert/swirl.inc"
#elif defined(DUCK)
#include "../objconvert/duck5.inc"
#endif


#define DEFAULT_FIFO_SIZE	(512*1024)

static void *xfb = NULL;
static u32 do_copy = GX_FALSE;
static GXRModeObj *rmode = NULL;

#define MAJOR_STEPS 128
#define MINOR_STEPS 64

#define SHADOW_WIDTH 256
#define SHADOW_HEIGHT 256

#define ARRAY_SIZE(X) ((sizeof (X)) / (sizeof ((X)[0])))

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

typedef struct
{
  guVector pos;
  guVector lookat;
  guVector up;

  guVector tpos;
  guVector tlookat;
} light_info;

static light_info light0 =
{
  .pos = { 20, 20, 30 },
  .tpos = { 0, 0, 0 },
  .lookat = { 0, 0, 0 },
  .tlookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

float lightdeg = 0.0;

#define ORTHO_SHADOW

float ortho_right = 20.0;
float ortho_top = 20.0;
#ifdef ORTHO_SHADOW
float shadow_near = 1.0;
#else
float shadow_near = 5.0;
#endif
float shadow_far = 200.0;

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
      /*GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
      GX_SetColorUpdate (GX_TRUE);
      GX_SetAlphaUpdate (GX_TRUE);*/
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
  
  GX_CopyDisp (framebuffer, GX_TRUE);
  GX_SetDispCopyGamma (GX_GM_1_0);

  return framebuffer;
}

static void
plain_lighting (void)
{
  GXLightObj lo;

#if 1
#include "plain-lighting.inc"
#else
  GX_SetNumTexGens (0);
  GX_SetNumChans (1);
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
  GX_SetNumTevStages (1);
#endif

  GX_SetChanAmbColor (GX_COLOR0A0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_InitLightPos (&lo, light0.tpos.x, light0.tpos.y, light0.tpos.z);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  // Initialise "a" parameters.
  GX_InitLightSpot (&lo, 0.0, GX_SP_OFF);
  // Initialise "k" parameters.
  GX_InitLightDistAttn (&lo, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightDir (&lo, 0.0, 0.0, 0.0);
  GX_LoadLightObj (&lo, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

/* Specular lighting needs two (channels?), apparently.
   This version works around a presumed libogc bug in GX_SetChanCtrl (fixed
   locally).  */

static void
specular_lighting (void)
{
  GXLightObj lo;
  guVector ldir;

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
  GX_InitLightPos (&lo, light0.tpos.x, light0.tpos.y, light0.tpos.z);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  // Initialise "a" parameters.
  GX_InitLightSpot (&lo, 0.0, GX_SP_OFF);
  // Initialise "k" parameters.
  GX_InitLightDistAttn (&lo, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightDir (&lo, 0.0, 0.0, 0.0);
  GX_LoadLightObj (&lo, GX_LIGHT0);

  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  /* Light 1: use for specular.  Should be able to use the same light for
     both!  I think.  */
  GX_InitSpecularDir (&lo, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo, 64);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  GX_LoadLightObj (&lo, GX_LIGHT1);

  GX_InvalidateTexAll ();
}

static void
specular_lighting_1light (void)
{
  GXLightObj lo;
  guVector ldir;

#if 1
#include "specular-lighting.inc"
#else
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
#endif

  GX_SetChanAmbColor (GX_COLOR0A0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_COLOR1A1, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_COLOR1A1, (GXColor) { 255, 255, 255, 0 });
  GX_SetChanCtrl (GX_COLOR1, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPEC);

  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  /* Light 0: use for both specular and diffuse lighting.  */
  GX_InitSpecularDir (&lo, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo, 64);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  GX_LoadLightObj (&lo, GX_LIGHT0);

  GX_InvalidateTexAll ();
}

static void
shadow_mapped_lighting (void)
{
  GXLightObj lo;

#if 1
#include "shadow-mapped-lighting.inc"
#else
  GX_SetNumTexGens (2);
  GX_SetNumChans (1);
  GX_SetNumTevStages (3);

  /* TEV stage 0: load depth value from ramp texture.  */
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
  GX_SetTevColorIn (GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
		    GX_CC_TEXC);
  GX_SetTevColorOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		    GX_TRUE, GX_TEVPREV);
  
  /* TEV stage 1: REGPREV >= shadow map texture ? 0 : 255.  */
  GX_SetTevOrder (GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLORNULL);
  GX_SetTevColorIn (GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ONE,
		    GX_CC_ZERO);
  GX_SetTevColorOp (GX_TEVSTAGE1, GX_TEV_COMP_R8_GT, GX_TB_ZERO, GX_CS_SCALE_1,
		    GX_FALSE, GX_TEVPREV);
  
  /* TEV stage 2: REGPREV == 0 ? shadow colour : rasterized colour.  */
  GX_SetTevOrder (GX_TEVSTAGE2, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GX_SetTevColorIn (GX_TEVSTAGE2, GX_CC_RASC, GX_CC_C0, GX_CC_CPREV,
		    GX_CC_ZERO);
  GX_SetTevColorOp (GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		    GX_TRUE, GX_TEVPREV);
#endif

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX1);
  
  /* TEV reg 0 has shadow colour!  */
  //GX_SetTevColor (GX_TEVREG0, (GXColor) { 255, 0, 0, 0 });
  GX_SetTevColor (GX_TEVREG0, (GXColor) { 4, 16, 4, 0 });
  
  /* Let's try with diffuse lighting.  */
  GX_SetChanAmbColor (GX_COLOR0A0, (GXColor) { 16, 32, 16, 0});
  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);
  
  GX_InitLightPos (&lo, light0.tpos.x, light0.tpos.y, light0.tpos.z);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  GX_InitLightSpot (&lo, 0.0, GX_SP_OFF);
  GX_InitLightDistAttn (&lo, 1.0, 1.0, GX_DA_OFF);
  GX_InitLightDir (&lo, 0.0, 0.0, 0.0);
  GX_LoadLightObj (&lo, GX_LIGHT0);
  
  GX_InvalidateTexAll ();
}

static void
specular_shadowed_lighting (void)
{
  GXLightObj lo;
  guVector ldir;

#include "specular-shadowed-lighting.inc"

  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX1);
  
  /* TEV reg 0 has shadow colour!  */
  //GX_SetTevColor (GX_TEVREG0, (GXColor) { 255, 0, 0, 0 });
  GX_SetTevColor (GX_TEVREG0, (GXColor) { 4, 16, 4, 0 });
  
  GX_SetChanAmbColor (GX_COLOR0A0, (GXColor) { 16, 32, 16, 0});
  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_COLOR1A1, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_COLOR1A1, (GXColor) { 255, 255, 255, 0 });
  GX_SetChanCtrl (GX_COLOR1, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_SPEC);

  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  /* Light 0: use for both specular and diffuse lighting.  */
  GX_InitSpecularDir (&lo, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo, 64);
  GX_InitLightColor (&lo, (GXColor) { 192, 192, 192, 0 });
  GX_LoadLightObj (&lo, GX_LIGHT0);
  
  GX_InvalidateTexAll ();
}

static void
shadow_depth (void)
{
  
  GX_SetTevColor (GX_TEVREG1, (GXColor) { 255, 0, 0, 0 });

#if 1
#include "shadow-depth.inc"
#else
  GX_SetNumTevStages (1);

  // TEV stage 0
  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
  GX_SetTevColorIn (GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C1, GX_CC_ZERO);
  GX_SetTevColorOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		    GX_TRUE, GX_TEVPREV);

  GX_SetNumChans (0);
  GX_SetNumTexGens (1);
#endif
  GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX0);
  GX_SetTexCoordGen (GX_TEXCOORD1, GX_TG_MTX3x4, GX_TG_POS, GX_TEXMTX1);

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
  
#ifdef SHIP
  GX_SetArray (GX_VA_POS, ship_pos, 3 * sizeof (f32));
  GX_SetArray (GX_VA_NRM, ship_norm, 3 * sizeof (f32));
#elif defined(SWIRL)
  GX_SetArray (GX_VA_POS, swirl_pos, 3 * sizeof (f32));
  GX_SetArray (GX_VA_NRM, swirl_norm, 3 * sizeof (f32));
#elif defined(DUCK)
  GX_SetArray (GX_VA_POS, duck5_pos, 3 * sizeof (f32));
  GX_SetArray (GX_VA_NRM, duck5_norm, 3 * sizeof (f32));
#else
  GX_SetArray (GX_VA_POS, torus, 3 * sizeof (f32));
  GX_SetArray (GX_VA_NRM, tornorms, 3 * sizeof (f32));
#endif
  //GX_SetArray (GX_VA_CLR0, colors, 4 * sizeof (u8));
  
  plain_lighting ();
}

static float deg = 0;
static float deg2 = 0;
static Mtx texproj, depth;

static void
render_obj (Mtx viewMatrix, int do_texture_mats, u16 **obj_strips,
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

#if defined(SHIP)
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

static void
update_screen (Mtx viewMatrix, int do_texture_mats)
{
  Mtx modelView, mvi, mvitmp;
  guVector axis = {0, 1, 0};
  int major, minor;

  guMtxIdentity (modelView);
  guMtxRotAxisDeg (modelView, &axis, deg);

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
  
  for (major = 0; major < MAJOR_STEPS; major++)
    {
      int mjidx = major * MINOR_STEPS;
      int mjidx1 = (major == MAJOR_STEPS - 1) ? 0
		  : (major + 1) * MINOR_STEPS;
      
      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, MINOR_STEPS * 2 + 2);

      GX_Position1x16 (mjidx);
      GX_Normal1x16 (mjidx);
      //GX_Color1x8 (0);
      GX_Position1x16 (mjidx1);
      GX_Normal1x16 (mjidx1);
      //GX_Color1x8 (0);

      for (minor = 1; minor <= MINOR_STEPS; minor++)
        {
	  int mnidx = (minor == MINOR_STEPS) ? 0 : minor;

	  GX_Position1x16 (mjidx + mnidx);
	  GX_Normal1x16 (mjidx + mnidx);
	  //GX_Color1x8 (0);
	  GX_Position1x16 (mjidx1 + mnidx);
	  GX_Normal1x16 (mjidx1 + mnidx);
	  //GX_Color1x8 (0);
	}
      GX_End();
    }
}

static void
update_anim (void)
{
  deg++;
  lightdeg += 0.5;
  deg2 += 0.5;
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
  Mtx viewmat, perspmat, lightortho, lightmat;
  guVector pos = {0, 0, 50};
  //guVector pos = { 50, 0, 0 };
  guVector up = {0, 1, 0};
  guVector lookat = {0, 0, 0};
  s32 ret;
  char localip[16] = {0};
  char gateway[16] = {0};
  char netmask[16] = {0};
  int light_model = 0;
  void *shadowtex;
  GXTexObj shadowtexobj;
  unsigned char *ramptex;
  GXTexObj ramptexobj;
  int i;

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
  guLookAt (viewmat, &pos, &up, &lookat);

#ifdef ORTHO_SHADOW
  guOrtho (lightortho, ortho_top, -ortho_top, -ortho_right, ortho_right,
	   shadow_near, shadow_far);
#else
  guFrustum (lightortho, ortho_top, -ortho_top, -ortho_right, ortho_right,
	     shadow_near, shadow_far);
#endif

  shadowtex = memalign (32, SHADOW_WIDTH * SHADOW_HEIGHT);

  GX_InitTexObj (&shadowtexobj, shadowtex, SHADOW_WIDTH, SHADOW_HEIGHT,
		 GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_InitTexObjLOD (&shadowtexobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0, GX_ANISO_1);

  ramptex = memalign (32, 16 * 16);
  
  for (i = 0; i < 256; i++)
    {
      /* Straight from the patent, hmm.  */
      unsigned int offset = ((i & 0x80) >> 2)
			    + ((i & 0x70) >> 4)
			    + ((i & 0x0c) << 4)
			    + ((i & 0x03) << 3);
      ramptex[offset] = i;
    }

  DCFlushRange (ramptex, 256);

  GX_InitTexObj (&ramptexobj, ramptex, 16, 16, GX_TF_I8, GX_CLAMP, GX_REPEAT,
		 GX_FALSE);
  GX_InitTexObjLOD (&ramptexobj, GX_NEAR, GX_NEAR, 0, 0, 0, 0, 0, GX_ANISO_1);

  draw_init ();

  while (1)
    {
      GX_InvVtxCache ();
      GX_InvalidateTexAll ();

      light0.pos.x = cos ((lightdeg * 4) / 180.0 * M_PI) * 30.0;
      light0.pos.y = cos (lightdeg / 180.0 * M_PI) * 25.0;
      light0.pos.z = sin (lightdeg / 180.0 * M_PI) * 25.0;
      //light0.pos.x = 50.0;
      //light0.pos.y = 0.0;
      //light0.pos.z = 0.0;

      guVecMultiply (viewmat, &light0.pos, &light0.tpos);
      guVecMultiply (viewmat, &light0.lookat, &light0.tlookat);

      {
	float near, far, range, tscale;
	Mtx dp, proj;

	/* Settings!  */
	near = shadow_near;
	far = shadow_far;

	range = far - near;
	tscale = 16.0f;

#ifdef ORTHO_SHADOW
	guLightOrtho (proj, -ortho_top, ortho_top, -ortho_right, ortho_right,
		      0.5f, 0.5f, 0.5f, 0.5f);

	guMtxIdentity (dp);
	guMtxScale (dp, 0.0f, 0.0f, 0.0f);

	/* We need:
            s = - (z + N) / ( F - N )
	    t = s * tscale
	*/
	guMtxRowCol (dp, 0, 2) = -1.0f / range;
	guMtxRowCol (dp, 0, 3) = -near / range;
	guMtxRowCol (dp, 1, 2) = guMtxRowCol (dp, 0, 2) * tscale;
	guMtxRowCol (dp, 1, 3) = guMtxRowCol (dp, 0, 3) * tscale;
	guMtxRowCol (dp, 2, 3) = 1.0f;
#else
	guLightFrustum (proj, -ortho_top, ortho_top, -ortho_right, ortho_right,
			near, 0.5f, 0.5f, 0.5f, 0.5f);

	guMtxIdentity (dp);
	guMtxScale (dp, 0.0f, 0.0f, 0.0f);
	
	guMtxRowCol (dp, 0, 2) = far / range;
	guMtxRowCol (dp, 0, 3) = far * near / range;
	guMtxRowCol (dp, 1, 2) = guMtxRowCol (dp, 0, 2) * tscale;
	guMtxRowCol (dp, 1, 3) = guMtxRowCol (dp, 0, 3) * tscale;
	guMtxRowCol (dp, 2, 2) = 1.0f;
#endif

	guLookAt (lightmat, &light0.pos, &light0.up, &light0.lookat);

	guMtxConcat (proj, lightmat, texproj);
	guMtxConcat (dp, lightmat, depth);
      }

      if (light_model >= 3 && light_model <= 5)
        {
	  GX_SetCullMode (GX_CULL_FRONT);
	  //GX_SetCullMode (GX_CULL_BACK);

#ifdef ORTHO_SHADOW
	  GX_LoadProjectionMtx (lightortho, GX_ORTHOGRAPHIC);
#else
	  GX_LoadProjectionMtx (lightortho, GX_PERSPECTIVE);
#endif

	  // GX_SetPixelFmt (GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	  /* We're only using 8 bits of Z precision, so set Z near/far
	     accordingly.  */
	  GX_SetViewport (0, 0, SHADOW_WIDTH, SHADOW_HEIGHT, 0, 1);
	  GX_SetScissor (0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);

	  /* Disable lighting &c.  */
	  GX_SetNumTexGens (0);
	  GX_SetNumChans (1);
	  GX_SetNumTevStages (1);
	  
	  GX_SetChanCtrl (GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG,
			  GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
	  GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL,
			  GX_COLOR0A0);
	  GX_SetTevOp (GX_TEVSTAGE0, GX_PASSCLR);
	  
	  GX_SetChanMatColor (GX_COLOR0A0, (GXColor) { 0, 0, 0, 0xc0 });

	  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
	  GX_SetColorUpdate (GX_FALSE);
	  GX_SetAlphaUpdate (GX_FALSE);

#ifdef SHIP
	  render_obj (lightmat, 0, ship_strips, ship_lengths,
		      ARRAY_SIZE (ship_strips));
#elif defined(SWIRL)
	  render_obj (lightmat, 0, swirl_strips, swirl_lengths,
		      ARRAY_SIZE (swirl_strips));
#elif defined(DUCK)
	  render_obj (lightmat, 0, duck5_strips, duck5_lengths,
		      ARRAY_SIZE (duck5_strips));
#else
	  update_screen (lightmat, 0);
#endif

	  GX_SetTexCopySrc (0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	  GX_SetTexCopyDst (SHADOW_WIDTH, SHADOW_HEIGHT, GX_TF_Z8, GX_FALSE);
	  GX_CopyTex (shadowtex, GX_TRUE);

	  /* We're supposed to call this before we try to use the texture we
	     just created.  */
	  GX_PixModeSync ();

	  GX_LoadTexObj (&ramptexobj, GX_TEXMAP0);
	  GX_LoadTexObj (&shadowtexobj, GX_TEXMAP1);
	}

      GX_SetCullMode (GX_CULL_BACK);
      
      GX_SetViewport (0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
      GX_SetScissor (0, 0, rmode->fbWidth, rmode->efbHeight);
      
      /* Set up texture transformation matrix for shadowing.  */
      GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);

      switch (light_model)
	{
	case 0:
	  plain_lighting ();
	  break;

	case 1:
	  specular_lighting ();
	  break;

	case 2:
	  specular_lighting_1light ();
	  break;

	case 3:
	  shadow_mapped_lighting ();
	  break;
	
	case 4:
	  specular_shadowed_lighting ();
	  break;
	
	case 5:
	  shadow_depth ();
	  break;
	}
      
      GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
      GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
      GX_SetColorUpdate (GX_TRUE);
      GX_SetAlphaUpdate (GX_TRUE);

#ifdef SHIP
      render_obj (viewmat, 1, ship_strips, ship_lengths,
		  ARRAY_SIZE (ship_strips));
#elif defined(SWIRL)
      render_obj (viewmat, 1, swirl_strips, swirl_lengths,
		  ARRAY_SIZE (swirl_strips));
#elif defined(DUCK)
      render_obj (viewmat, 1, duck5_strips, duck5_lengths,
		  ARRAY_SIZE (duck5_strips));
#else
      update_screen (viewmat, 1);
#endif

      GX_DrawDone ();
      do_copy = GX_TRUE;
      VIDEO_WaitVSync();

      PAD_ScanPads();

      int buttonsDown = PAD_ButtonsDown (0);

      if (buttonsDown & PAD_BUTTON_START)
        return_to_loader ();

      if (buttonsDown & PAD_BUTTON_A)
        {
	  light_model++;
	  
	  if (light_model > 5)
	    light_model = 0;
	}

      update_anim ();
    }

  return 0;
}
