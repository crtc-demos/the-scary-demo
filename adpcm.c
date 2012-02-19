#include <ogcsys.h>
#include <gccore.h>
#include <gctypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aesndlib.h>
#include <string.h>
#include <malloc.h>

#include "server.h"
#include "adpcm.h"

typedef struct
{
  u32 aram_handle;
  u32 block_idx[2];
  u32 block_valid[2];
  unsigned char *block[2];
  int front_buf;
} aram_buffer_info;

#define ARAM_CACHE_BLOCKSIZE 16384

static aram_buffer_info aram_buffer;

/* Start with pre-loaded blocks (seems easier...).  */

void
aram_cache_init (aram_buffer_info *abuf, u32 aram_handle, u32 start_idx)
{
  u32 start_block = start_idx & ~(ARAM_CACHE_BLOCKSIZE - 1);

  if (start_idx == 0)
    srv_printf ("Init ARAM cache...\n");
  else
    srv_printf ("Reinit ARAM cache (start block at offset %u)\n", start_block);
  abuf->aram_handle = aram_handle;
  abuf->block_idx[0] = start_block;
  abuf->block_idx[1] = start_block + ARAM_CACHE_BLOCKSIZE;
  abuf->block_valid[0] = abuf->block_valid[1] = 1;
  if (start_idx == 0)
    {
      abuf->block[0] = memalign (32, ARAM_CACHE_BLOCKSIZE);
      abuf->block[1] = memalign (32, ARAM_CACHE_BLOCKSIZE);
    }
  DCInvalidateRange (abuf->block[0], ARAM_CACHE_BLOCKSIZE);
  AR_StartDMA (AR_ARAMTOMRAM, (u32) abuf->block[0], aram_handle + start_block,
	       ARAM_CACHE_BLOCKSIZE);
  while (AR_GetDMAStatus () != 0);
  DCInvalidateRange (abuf->block[1], ARAM_CACHE_BLOCKSIZE);
  AR_StartDMA (AR_ARAMTOMRAM, (u32) abuf->block[1],
	       aram_handle + start_block + ARAM_CACHE_BLOCKSIZE,
	       ARAM_CACHE_BLOCKSIZE);
  while (AR_GetDMAStatus () != 0);
  abuf->front_buf = 0;
}

static unsigned char
aram_read (aram_buffer_info *abuf, int offset)
{
  int active = abuf->front_buf;
  int back = 1 - active;
  unsigned int active_start = abuf->block_idx[active];
  unsigned int active_end = active_start + ARAM_CACHE_BLOCKSIZE;
  unsigned int back_start = abuf->block_idx[back];
  unsigned int back_end = back_start + ARAM_CACHE_BLOCKSIZE;

  /* If we need data that has not been DMA'd yet, wait until the DMA
     finishes.  Only the back block can be invalid here.  This is never
     expected to trigger, in practice.  */
  if (offset >= back_start && offset < back_end
      && !abuf->block_valid[back])
    {
      while (AR_GetDMAStatus () != 0);
      abuf->block_valid[back] = 1;
    }

  if (offset >= active_start && offset < active_end)
    {
      if (!abuf->block_valid[active])
        srv_printf ("active block not valid?\n");

      return abuf->block[active][offset - active_start];
    }
  else if (offset >= back_start && offset < back_end)
    {
      unsigned int next_block = (offset & ~(ARAM_CACHE_BLOCKSIZE - 1))
				+ ARAM_CACHE_BLOCKSIZE;
      
      active = 1 - active;
      back = 1 - back;
      active_start = abuf->block_idx[active];

      /* Valid back buffer.  Make it the front buffer.  */
      abuf->front_buf = active;

      /*srv_printf ("DMA next block, offset %d\n", next_block);*/

      /* ...and load the next block into the previous front buffer.  */
      DCInvalidateRange (abuf->block[back], ARAM_CACHE_BLOCKSIZE);
      AR_StartDMA (AR_ARAMTOMRAM, (u32) abuf->block[back],
		   abuf->aram_handle + next_block, ARAM_CACHE_BLOCKSIZE);
      
      abuf->block_valid[back] = 0;
      abuf->block_idx[back] = next_block;
      
      return abuf->block[active][offset - active_start];
    }
  else
    srv_printf ("unexpected offset, %d\n", offset);

  return 0;
}

typedef struct
{
  int format;
  int channels;
  int sample_rate;
  int avg_bytes_per_sec;
  int block_align;
  int bits_per_sample;
  int data_length;
} waveformatex;

typedef struct
{
  int block_predictor;
  int idelta;
  int sample1;
  int sample2;
} adpcm_state;

typedef struct
{
  waveformatex header;
  adpcm_state left, right;
  int idx;
  int block_remaining;
  int length;
} playback_state;

static playback_state playback_state_0;

static int
get_halfword (aram_buffer_info *abuf, int offset)
{
  unsigned int b1 = aram_read (abuf, offset);
  unsigned int b2 = aram_read (abuf, offset + 1);
  return b1 + b2 * 256;
}

static int
get_word (aram_buffer_info *abuf, int offset)
{
  unsigned int b1 = aram_read (abuf, offset);
  unsigned int b2 = aram_read (abuf, offset + 1);
  unsigned int b3 = aram_read (abuf, offset + 2);
  unsigned int b4 = aram_read (abuf, offset + 3);
  return b1 + (b2 << 8) + (b3 << 16) + (b4 << 24);
}

static u32
read_header (aram_buffer_info *abuf, playback_state *ps, u32 skip_millis)
{
  unsigned int ssize;
  int idx;
  waveformatex header;
  u32 frames;

  idx = 12;

  srv_printf ("subchunk 1 id: %c %c %c %c\n", aram_read (abuf, idx),
	      aram_read (abuf, idx + 1), aram_read (abuf, idx + 2),
	      aram_read (abuf, idx + 3));
  ssize = get_word (abuf, idx + 4);
  srv_printf ("subchunk 1 size: %d\n", ssize);
  header.format = get_halfword (abuf, idx + 8);
  srv_printf ("format: %d\n", header.format);
  header.channels = get_halfword (abuf, idx + 10);
  srv_printf ("num channels: %d\n", header.channels);
  header.sample_rate = get_word (abuf, idx + 12);
  srv_printf ("sample rate: %d\n", header.sample_rate);
  header.avg_bytes_per_sec = get_word (abuf, idx + 16);
  srv_printf ("average bytes per sec: %d\n", header.avg_bytes_per_sec);
  header.block_align = get_halfword (abuf, idx + 20);
  srv_printf ("block align (adpcm block size): %d\n", header.block_align);
  header.bits_per_sample = get_halfword (abuf, idx + 22);
  srv_printf ("bits per sample: %d\n", header.bits_per_sample);
  idx += ssize + 8;
  
  srv_printf ("subchunk 2 id: %c %c %c %c\n", aram_read (abuf, idx),
	      aram_read (abuf, idx + 1), aram_read (abuf, idx + 2),
	      aram_read (abuf, idx + 3));
  ssize = get_word (abuf, idx + 4);
  srv_printf ("subchunk 2 size: %d\n", ssize);
  srv_printf ("value: %x (\?\?\?)\n", get_word (abuf, idx + 8));
  idx += ssize + 8;
  
  srv_printf ("subchunk 3 id: %c %c %c %c\n", aram_read (abuf, idx),
	      aram_read (abuf, idx + 1), aram_read (abuf, idx + 2),
	      aram_read (abuf, idx + 3));
  header.data_length = get_word (abuf, idx + 4);
  srv_printf ("subchunk 3 size: %d\n", header.data_length);
  idx += 8;
  
  /* Assumes one byte per (left+right) sample...  */
  frames = (header.sample_rate * skip_millis)
	   / (1000 * (header.block_align - 14));
  
  if (frames > 0)
    srv_printf ("Skipping %u frames\n", frames);
  
  ps->idx = idx + header.block_align * frames;
  ps->header = header;
  
  return skip_millis;
}

static int
extend_16bits (unsigned int in)
{
  if (in < 32768)
    return in;
  else
    return (int) in - 65536;
}

static int
get_signed_halfword (aram_buffer_info *abuf, int offset)
{
  return extend_16bits (get_halfword (abuf, offset));
}

static void
decode_preamble (aram_buffer_info *abuf, playback_state *ps)
{
  int idx = ps->idx;

  ps->left.block_predictor = aram_read (abuf, idx);
  ps->right.block_predictor = aram_read (abuf, idx + 1);
  ps->left.idelta = get_signed_halfword (abuf, idx + 2);
  ps->right.idelta = get_signed_halfword (abuf, idx + 4);
  ps->left.sample1 = get_signed_halfword (abuf, idx + 6);
  ps->right.sample1 = get_signed_halfword (abuf, idx + 8);
  ps->left.sample2 = get_signed_halfword (abuf, idx + 10);
  ps->right.sample2 = get_signed_halfword (abuf, idx + 12);

  ps->idx = idx + 14;
}

const static int adaptation_table[] = {
  230, 230, 230, 230, 307, 409, 512, 614,
  768, 614, 512, 409, 307, 230, 230, 230
};

const static int adapt_coeff_1[] = {
  256, 512, 0, 192, 240, 460, 392
};

const static int adapt_coeff_2[] = {
  0, -256, 0, 64, 0, -208, -232
};

static int
get_sample (adpcm_state *state, int in_sample)
{
  int coeff1 = adapt_coeff_1[state->block_predictor];
  int coeff2 = adapt_coeff_2[state->block_predictor];
  int out_sample = state->sample2;
  int predictor = (state->sample1 * coeff1 + state->sample2 * coeff2) / 256;
  unsigned int in_sample_uns = in_sample >= 0 ? in_sample : in_sample + 16;
  
  predictor += in_sample * state->idelta;
  
  if (predictor < -32768)
    predictor = -32768;
  else if (predictor > 32767)
    predictor = 32767;
  
  state->sample2 = state->sample1;
  state->sample1 = predictor;
  
  state->idelta = (adaptation_table[in_sample_uns] * state->idelta) / 256;
  
  if (state->idelta < 16)
    state->idelta = 16;
  
  return out_sample;
}

static int
extend_4bits (unsigned int in)
{
  if (in < 8)
    return in;
  else
    return (int) in - 16;
}

#define MAX_NUM_BLOCKS 1

static u32 aram_blocks[MAX_NUM_BLOCKS];
static bool sndPlaying = false;
static bool thr_running = false;

void
adpcm_init (void)
{
  AR_Init (aram_blocks, MAX_NUM_BLOCKS);
  sndPlaying = false;
  thr_running = false;
}

#define LOAD_BLOCKSIZE 1024

static int adpcm_next_sample (aram_buffer_info *abuf, playback_state *ps,
			      int *lsamp_out, int *rsamp_out);

u32
adpcm_load_file (const char *filename)
{
  struct stat buf;
  FILE *fh;
  u32 song;
  unsigned char *mem = memalign (32, LOAD_BLOCKSIZE);
  size_t nread, total = 0;
  unsigned int song_length;
  int sometimes = 0;

  stat (filename, &buf);
  song_length = buf.st_size;
  
  song = AR_Alloc (song_length);
  
  if (!song)
    {
      srv_printf ("Can't allocate ARAM\n");
      return 0;
    }
  
  fh = fopen (filename, "r");

  srv_printf ("Loading '%s' to ARAM (%d bytes)", filename, song_length);
  do
    {
      nread = fread (mem, 1, LOAD_BLOCKSIZE, fh);
      DCFlushRange (mem, LOAD_BLOCKSIZE);
      AR_StartDMA (AR_MRAMTOARAM, (u32) mem, song + total, (nread + 31) & ~31);
      /* Busy-wait.  This is the slowest possible way of doing things!  */
      while (AR_GetDMAStatus () != 0);
      total += nread;
      sometimes++;
      if (sometimes > 1023)
        {
          srv_printf (".");
	  sometimes = 0;
	}
    }
  while (total < song_length);

  fclose (fh);
  
  free (mem);
  
  srv_printf ("\nDone.\n");

  playback_state_0.length = song_length;
  playback_state_0.block_remaining = 0;
  playback_state_0.idx = 0;

#if 0
  aram_cache_init (&aram_buffer, song);
  read_header (&aram_buffer, &playback_state_0);
  srv_printf ("Writing raw wave...\n");
  fh = fopen ("sd:/raw-wave.out", "w");
  {
    int res;
    do
      {
	int lsamp, rsamp;
	res = adpcm_next_sample (&aram_buffer, &playback_state_0,
				 &lsamp, &rsamp);
	if (res == 0)
	  {
	    fputc (lsamp & 255, fh);
	    fputc ((lsamp >> 8) & 255, fh);
	    fputc (rsamp & 255, fh);
	    fputc ((rsamp >> 8) & 255, fh);
	  }
      } while (res == 0);
  }
  srv_printf ("Done.\n");
  fclose (fh);
#endif

  return song;
}

#define STACKSIZE 8192
#define SNDBUFFERSIZE 5760

static vu32 curr_audio = 0;
static u8 audioBuf[2][SNDBUFFERSIZE] ATTRIBUTE_ALIGN(32);
static lwpq_t player_queue;
static lwp_t hplayer;
static u8 player_stack[STACKSIZE];
static vu32 filling = 0;

static int
adpcm_next_sample (aram_buffer_info *abuf, playback_state *ps, int *lsamp_out,
		   int *rsamp_out)
{
  unsigned char samp;
  int lsamp, rsamp;

  if (ps->block_remaining == 0)
    {
      int start_idx = ps->idx;
      decode_preamble (abuf, ps);
      ps->block_remaining = ps->header.block_align - (ps->idx - start_idx);
    }
  
  if (ps->idx >= ps->length)
    return 1;
  
  samp = aram_read (abuf, ps->idx++);
  
  lsamp = extend_4bits (samp >> 4);
  rsamp = extend_4bits (samp & 15);
  
  *lsamp_out = get_sample (&ps->left, lsamp);
  *rsamp_out = get_sample (&ps->right, rsamp);
  
  ps->block_remaining--;
  
  return 0;
}

static void *
player (void *arg)
{
  thr_running = true;
  while (sndPlaying)
    {
      LWP_ThreadSleep (player_queue);
      if (sndPlaying)
        {
	  int i;
	  filling = true;
	  for (i = 0; i < SNDBUFFERSIZE / 4; i++)
	    {
	      int lsamp = 0, rsamp = 0;
	      adpcm_next_sample (&aram_buffer, &playback_state_0, &lsamp,
				 &rsamp);
	      audioBuf[curr_audio][i * 4] = lsamp >> 8;
	      audioBuf[curr_audio][i * 4 + 1] = lsamp & 255;
	      audioBuf[curr_audio][i * 4 + 2] = rsamp >> 8;
	      audioBuf[curr_audio][i * 4 + 3] = rsamp & 255;
	    }
	  filling = false;
	}
    }

  thr_running = false;
  
  return 0;
}

static void
adpcm_voice_callback (AESNDPB *pb, u32 state)
{
  switch (state)
    {
    case VOICE_STATE_STOPPED:
    case VOICE_STATE_RUNNING:
      break;
    
    case VOICE_STATE_STREAM:
      if (filling)
        srv_printf ("Underrun detected!\n");
      AESND_SetVoiceBuffer (pb, (void*) audioBuf[curr_audio], SNDBUFFERSIZE);
      curr_audio ^= 1;
      LWP_ThreadSignal (player_queue);
    }
}

static AESNDPB *adpcm_voice;

u32
adpcm_play (u32 handle, u32 skip_to_millis)
{
  u32 adjusted_millis;

  if (sndPlaying)
    return skip_to_millis;

  adpcm_voice = AESND_AllocateVoice (adpcm_voice_callback);
  if (adpcm_voice)
    {
      AESND_SetVoiceFormat (adpcm_voice, VOICE_STEREO16);
      AESND_SetVoiceFrequency (adpcm_voice, 44100);
      AESND_SetVoiceVolume (adpcm_voice, 255, 255);
      AESND_SetVoiceStream (adpcm_voice, true);
    }
  else
    {
      srv_printf ("No voice?\n");
      return skip_to_millis;
    }
  
  LWP_InitQueue (&player_queue);
  
  memset (audioBuf[0], 0, SNDBUFFERSIZE);
  memset (audioBuf[1], 0, SNDBUFFERSIZE);
  
  DCFlushRange (audioBuf[0], SNDBUFFERSIZE);
  DCFlushRange (audioBuf[1], SNDBUFFERSIZE);
  
  aram_cache_init (&aram_buffer, handle, 0);
  
  adjusted_millis = read_header (&aram_buffer, &playback_state_0,
				 skip_to_millis);
  
  aram_cache_init (&aram_buffer, handle, playback_state_0.idx);
  
  while (thr_running);
  
  curr_audio = 0;
  sndPlaying = true;

  if (LWP_CreateThread (&hplayer, player, NULL, player_stack, STACKSIZE, 80)
      != -1)
    {
      AESND_SetVoiceStop (adpcm_voice, false);
      return skip_to_millis;
    }

  sndPlaying = false;
  
  return adjusted_millis;
}
