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
#include <errno.h>
#include <ext2.h>
#include <sdcard/gcsd.h>
#include <dirent.h>

#include <ogcsys.h>
#include <gccore.h>
#include <gctypes.h>
#include <network.h>
#include <debug.h>
#include <malloc.h>

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
#include "tentacles.h"
#include "sintab.h"

/* Not in any header file AFAICT...  */
extern u64 gettime (void);
extern u32 diff_msec (u64 start, u64 end);

#undef HOLD
#undef DEBUG

#undef PLAY_MP3
#define PLAY_MOD

#ifdef PLAY_MP3
#include <asndlib.h>
#include <mp3player.h>
#endif

#ifdef PLAY_MOD
#include <aesndlib.h>
#include "back_to_my_roots_mod.h"
//#include "to_back_xm.h"
//#include "its_3_a_e_a_m_mod.h"
#endif

#undef SKIP_TO_TIME
//#define SKIP_TO_TIME 95000

#ifdef SKIP_TO_TIME
u64 offset_time = 0;
#else
#define offset_time 0
#endif

#define DEFAULT_FIFO_SIZE	(512*1024)

uint64_t start_time;

static do_thing_at sequence[] = {
 /*{      0, 300000, &parallax_mapping_methods, &parallax_mapping_data_0, -1, 0 }*/
  /*{      0, 50000, &tentacle_methods, &tentacle_data_0, -1, 0 },*/
  /*{      0,  15000, &glass_methods, &glass_data_0, -1, 0 },
  {  15000,  50000, &bloom_methods, &bloom_data_0, -1, 0 },
  {  50000,  70000, &pumpkin_methods, &pumpkin_data_0, -1, 0 },
  {  70000,  95000, &soft_crtc_methods, NULL, -1, 0 },
  {  95000, 110000, &tubes_methods, NULL, -1, 0 },
  { 110000, 300000, &spooky_ghost_methods, &spooky_ghost_data_0, -1, 0 }*/
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

#define MAX_ARAM_BLOCKS 1
u32 aram_blocks[MAX_ARAM_BLOCKS];

#ifdef PLAY_MP3
static s32
mp3_reader (void *data, void* readstart, s32 readsize)
{
  FILE *fh = (FILE *) data;
  size_t bytesread;
  
  // srv_printf ("mp3 reader callback: requesting %d bytes\n", (int) readsize);
  
  bytesread = fread (readstart, 1, readsize, fh);
  
  return bytesread;
}
#endif

int
main (int argc, char *argv[])
{
  int quit = 0;
  u64 start_time;
  int i;
  unsigned int next_effect;
  do_thing_at *active_effects[MAX_ACTIVE];
  display_target target_for_active_effect[MAX_ACTIVE];
  backbuffer_info backbuffers[MAX_BACKBUFFERS];
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
#ifdef PLAY_MOD
  MODPlay modplay;
#endif
#ifdef PLAY_MP3
  FILE *mp3_fh = NULL;
#endif

  memset (backbuffers, 0, sizeof (backbuffers));

  /* Initialise global tables.  */
  fastsin_init ();

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

#ifdef PLAY_MP3
  /*srv_printf ("ARAM init\n");
  AR_Init (aram_blocks, MAX_ARAM_BLOCKS);*/
    
  do {
    struct stat buf;
    int ret;
    sec_t *partitions;
    int parts;
    
    if ((parts = ext2FindPartitions (&__io_gcsda, &partitions)) == -1)
      {
        srv_printf ("ext2FindPartitions error\n");
	break;
      }

    srv_printf ("Found %d partitions\n", parts);
    
    if (ext2Mount ("sd", &__io_gcsda, partitions[0],
		   EXT2_CACHE_DEFAULT_PAGE_COUNT, EXT2_CACHE_DEFAULT_PAGE_SIZE,
		   EXT2_FLAG_DEFAULT))
      srv_printf ("Mounted ext2 OK.\n");
    else
      {
        srv_printf ("Mounting ext2 failed!\n");
	break;
      }
    
    /*if (fatInitDefault ())
      srv_printf ("FAT init success\n");
    else
      srv_printf ("FAT init failure\n");*/
    
    /*if (fatMountSimple ("sd", &__io_gcsda))
      srv_printf ("FAT mount returned non-zero (mounted?)\n");
    else
      srv_printf ("FAT mount returned zero\n");*/
    
    srv_printf ("files in sd:/...\n");
    
    {
      DIR *dir = opendir ("sd:/");
      
      if (!dir)
        srv_printf ("opendir returned NULL\n");
      
      while (1)
        {
	  struct dirent *ent = readdir (dir);
	  
	  if (!ent)
	    break;
	  
	  srv_printf ("%s\n", ent->d_name);
	}
      
      closedir (dir);
    }
    
    mp3_fh = fopen ("sd:/test2.mp3", "r");
    
    if (mp3_fh != 0)
      {
	srv_printf ("opened file\n");

	ret = fstat (fileno (mp3_fh), &buf);

	if (ret != 0)
	  srv_printf ("stat gave error, '%s'\n", strerror (errno));
	else
	  srv_printf ("size of stat'd file: %d\n", (int) buf.st_size);

	/*fclose (fh);*/

	srv_printf ("Init ASND\n");
	ASND_Init ();
	
	srv_printf ("Init MP3 player\n");
	MP3Player_Init ();
	
	MP3Player_PlayFile (mp3_fh, mp3_reader, NULL);
      }
    else
      srv_printf ("couldn't open file\n");
  } while (0);
#endif

#ifdef PLAY_MOD
  srv_printf ("AESND Init\n");

  AESND_Init (NULL);

  srv_printf ("Initialising MOD playback...\n");

  MODPlay_Init (&modplay);
  ret = MODPlay_SetFrequency (&modplay, 48000);
  srv_printf ("  (Set frequency... %s)\n", ret == 0 ? "successful" : "failed");
  MODPlay_SetVolume (&modplay, 64, 64);
#if 1
  ret = MODPlay_SetMOD (&modplay, back_to_my_roots_mod_size,
			back_to_my_roots_mod);
#else
  ret = MODPlay_SetMOD (&modplay, its_3_a_e_a_m_mod_size,
			its_3_a_e_a_m_mod);
#endif
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
      int compositors_active;

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
		    active_effects[i]->params, backbuffers);
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
		    sequence[next_effect].params, backbuffers);
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

      for (compositors_active = 0, i = 0; i < num_active_effects; i++)
        if (active_effects[i]->methods->composite_effect)
	  compositors_active++;

#ifdef DEBUG
      srv_printf ("prepare frame (active effects=%d)\n", num_active_effects);
#endif
      
      for (i = 0; i < num_active_effects; i++)
        {
	  if (active_effects[i]->methods->prepare_frame)
	    {
	      int target;
#ifdef DEBUG
	      srv_printf ("prepare frame: %p\n",
		      active_effects[i]->methods->prepare_frame);
#endif
	      target = active_effects[i]->methods->prepare_frame (
		    current_time - active_effects[i]->start_time,
		    active_effects[i]->params, active_effects[i]->iparam);

	      /* If there aren't any compositing effects active, render direct
		 to the main buffer.  */
	      if (compositors_active == 0)
	        target = MAIN_BUFFER;

	      target_for_active_effect[i] = target;
	    }
	}

#ifdef DEBUG
      srv_printf ("begin frame\n");
#endif

      /* Render back-buffer screens.  */

      for (j = BACKBUFFER_0; j <= BACKBUFFER_3; j++)
        {
	  int idx = j - BACKBUFFER_0;

	  if (!backbuffers[idx].initialized)
	    continue;

	  rendertarget_texture (backbuffers[idx].width, backbuffers[idx].height,
				backbuffers[idx].copyfmt);
	  
          for (i = 0; i < num_active_effects; i++)
	    if (active_effects[i]->methods->display_effect
	        && target_for_active_effect[i] == j)
	      active_effects[i]->methods->display_effect (
	        current_time - active_effects[i]->start_time,
		active_effects[i]->params, active_effects[i]->iparam);
	  
	  GX_CopyTex (backbuffers[idx].pixels, GX_TRUE);
	}

      GX_PixModeSync ();

      rendertarget_screen (rmode);

      /* If we're not using compositing, render the set of currently-active
         effects to the screen.  */
      for (i = 0; i < num_active_effects; i++)
	if (active_effects[i]->methods->display_effect
	    && target_for_active_effect[i] == MAIN_BUFFER)
	  active_effects[i]->methods->display_effect (
	    current_time - active_effects[i]->start_time,
	    active_effects[i]->params, active_effects[i]->iparam);

      /* And if we are using compositing, do that.  */
      for (i = 0; i < num_active_effects; i++)
	if (active_effects[i]->methods->composite_effect)
	  active_effects[i]->methods->composite_effect (
	    current_time - active_effects[i]->start_time,
	    active_effects[i]->params, active_effects[i]->iparam,
	    backbuffers);

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

#ifdef PLAY_MOD
  MODPlay_Stop (&modplay);
#endif

#ifdef PLAY_MP3
  MP3Player_Stop ();
  if (mp3_fh)
    fclose (mp3_fh);
  ASND_End ();
  srv_printf ("Unmount ext2\n");
  ext2Unmount ("sd");
#endif

  return_to_loader ();

  return 0;
}
