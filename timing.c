/* Demo timing harness.  */

#include <math.h>
#include <stdlib.h>
#include <alloca.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <debug.h>
#include <malloc.h>

#include <aesndlib.h>
#include <gcmodplay.h>

#include "server.h"
#include "timing.h"
#include "rendertarget.h"
#include "soft-crtc.h"
#include "tubes.h"
#include "spooky-ghost.h"
#include "pumpkin.h"
#include "bloom.h"
#include "glass.h"
#include "parallax-mapping.h"

/* Not in any header file AFAICT...  */
extern u64 gettime (void);
extern u32 diff_msec (u64 start, u64 end);

#undef HOLD
#undef DEBUG

#undef SOUND

#ifdef SOUND
#include "back_to_my_roots_mod.h"
#endif

//#undef SKIP_TO_TIME
#define SKIP_TO_TIME 50000

#ifdef SKIP_TO_TIME
u64 offset_time = 0;
#else
#define offset_time 0
#endif

#define DEFAULT_FIFO_SIZE	(512*1024)

uint64_t start_time;

static do_thing_at sequence[] = {
 /*{      0, 300000, &parallax_mapping_methods, NULL, -1, 0 }*/
  {      0,  15000, &glass_methods, NULL, -1, 0 },
  {  15000,  50000, &bloom_methods, NULL, -1, 0 },
  {  50000,  70000, &pumpkin_methods, NULL, -1, 0 },
  {  70000,  95000, &soft_crtc_methods, NULL, -1, 0 },
  {  95000, 110000, &tubes_methods, NULL, -1, 0 },
  { 110000, 300000, &spooky_ghost_methods, NULL, -1, 0 }
};

#define ARRAY_SIZE(X) (sizeof (X) / sizeof (X[0]))

#define MAX_ACTIVE 20

static void *xfb = NULL;
static u32 do_copy = GX_FALSE;
static GXRModeObj *rmode = NULL;

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

void *
initialise ()
{
  void *framebuffer;
  void *gp_fifo;
  GXColor background = {0, 0, 0, 0};

  VIDEO_Init();
  PAD_Init();

  //rmode = VIDEO_GetPreferredMode (NULL);
  rmode = &TVPal528IntDf;
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
draw_init (void)
{
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX16);
  GX_SetVtxDesc (GX_VA_NRM, GX_INDEX16);
  
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
}

static void
return_to_loader (void)
{
  void (*reload)() = (void(*)()) 0x80001800;
  srv_disconnect ();
  reload ();
}

extern int switch_ghost_lighting;

int
main (int argc, char *argv[])
{
  int quit = 0;
  u64 start_time;
  int i;
  unsigned int next_effect;
  do_thing_at *active_effects[MAX_ACTIVE];
  unsigned int num_active_effects;
  const int num_effects = ARRAY_SIZE (sequence);
  char localip[16] = {0};
  char gateway[16] = {0};
  char netmask[16] = {0};
  s32 ret;
  Mtx viewmat, perspmat;
  guVector pos = {0, 0, 50};
  guVector up = {0, 1, 0};
  guVector lookat = {0, 0, 0};
#ifdef SOUND
  MODPlay modplay;
#endif

  xfb = initialise ();

  /* Hopefully this will trigger if we exit unexpectedly...  */
  atexit (return_to_loader);

  printf ("Configuring network ...\n");

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
  srv_printf ("rmode = %p\n", (void*) rmode);

  guPerspective (perspmat, 60, 1.33f, 10.0f, 300.0f);
  guLookAt (viewmat, &pos, &up, &lookat);

  draw_init ();

  for (i = 0; i < ARRAY_SIZE (sequence); i++)
    {
      if (sequence[i].methods->preinit_assets)
        sequence[i].methods->preinit_assets ();
    }

  num_active_effects = 0;
  next_effect = 0;

#ifdef SOUND
  srv_printf ("AESND Init\n");

  AESND_Init (NULL);

  srv_printf ("Initialising MOD playback...\n");

  MODPlay_Init (&modplay);
  ret = MODPlay_SetFrequency (&modplay, 48000);
  srv_printf ("  (Set frequency... %s)\n", ret == 0 ? "successful" : "failed");
  ret = MODPlay_SetMOD (&modplay, back_to_my_roots_mod);
  srv_printf ("  (Set MOD... %s)\n", ret == 0 ? "successful" : "failed");
  ret = MODPlay_Start (&modplay);
  srv_printf ("  (Start playback... %s)\n", ret == 0 ? "successful" : "failed");
#endif

  start_time = gettime ();

#ifdef SKIP_TO_TIME
  offset_time = SKIP_TO_TIME;
#endif

  while (!quit)
    {
      u32 current_time;
      int i, j;

      GX_InvVtxCache ();
      GX_InvalidateTexAll ();

      current_time = diff_msec (start_time, gettime ()) + offset_time;
      // srv_printf ("current_time: %d\n", current_time);

      /* Terminate old effects.  */
      for (i = 0; i < num_active_effects; i++)
        {
	  if (current_time >= active_effects[i]->end_time)
	    {
	      /*srv_printf ("uninit effect %d (iparam=%d)\n", i,
		      active_effects[i]->iparam);*/

	      if (active_effects[i]->methods->uninit_effect)
	        {
	          active_effects[i]->methods->uninit_effect (
		    active_effects[i]->params);
		}
	      /* If we're not going to use this one any more, release any
	         global resources (etc.).  */
	      if (active_effects[i]->finalize
		  && active_effects[i]->methods->finalize)
		{
		  active_effects[i]->methods->finalize (
		    active_effects[i]->params);
		}
	      active_effects[i] = NULL;
	    }
	}

      /* And remove from active list.  */
      for (i = 0, j = 0; j < num_active_effects;)
        {
	  active_effects[i] = active_effects[j];

	  if (active_effects[i] == NULL)
	    j++;
	  else
	    {
	      i++;
	      j++;
	    }
	}

      num_active_effects = i;

#ifdef DEBUG
      srv_printf ("start new effects\n");
#endif

      while (next_effect < num_effects
	     && current_time >= sequence[next_effect].start_time)
	{
	  /* Only start the effect if it's before its ending time.  */
	  if (current_time < sequence[next_effect].end_time)
	    {
	      active_effects[num_active_effects] = &sequence[next_effect];

	      /*srv_printf ("init effect %d (%p, iparam=%d)\n", next_effect,
		      sequence[next_effect].methods->init_effect,
		      sequence[next_effect].iparam);*/

	      if (sequence[next_effect].methods->init_effect)
		{
		  sequence[next_effect].methods->init_effect (
		    sequence[next_effect].params);
		}

	      num_active_effects++;
	    }
	  else if (sequence[next_effect].finalize)
	    {
	      /* We must have skipped over an effect: finalize it now.  */
	      if (sequence[next_effect].methods->finalize)
	        {
	          sequence[next_effect].methods->finalize (
		    sequence[next_effect].params);
		}
	    }

	  next_effect++;
	}

      if (next_effect == num_effects && num_active_effects == 0)
        quit = 1;

      /* Do things we need to do before starting to send stuff to the PVR.  */

#ifdef DEBUG
      srv_printf ("prepare frame (active effects=%d)\n", num_active_effects);
#endif
      
      for (i = 0; i < num_active_effects; i++)
        {
	  if (active_effects[i]->methods->prepare_frame)
	    {
#ifdef DEBUG
	      srv_printf ("prepare frame: %p\n",
		      active_effects[i]->methods->prepare_frame);
#endif
	      active_effects[i]->methods->prepare_frame (
		current_time - active_effects[i]->start_time,
		active_effects[i]->params, active_effects[i]->iparam);
	    }
	}

#ifdef DEBUG
      srv_printf ("begin frame\n");
#endif

      rendertarget_screen (rmode);

      for (i = 0; i < num_active_effects; i++)
        {
	  if (active_effects[i]->methods->display_effect)
	    active_effects[i]->methods->display_effect (
	      current_time - active_effects[i]->start_time,
	      active_effects[i]->params, active_effects[i]->iparam, rmode);
	}

      GX_DrawDone ();
      do_copy = GX_TRUE;
      VIDEO_WaitVSync();

      PAD_ScanPads();

      int buttonsDown = PAD_ButtonsDown (0);

      if (buttonsDown & PAD_BUTTON_START)
        quit = 1;

      if (buttonsDown & PAD_BUTTON_A)
        switch_ghost_lighting = 1 - switch_ghost_lighting;

#ifdef DEBUG
      srv_printf ("finished frame\n");
#endif
    }

#ifdef SOUND
  MODPlay_Stop (&modplay);
#endif

  return_to_loader ();

  return 0;
}
