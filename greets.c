#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "timing.h"
#include "greets.h"
#include "screenspace.h"
#include "spooky-ghost.h"

#include "images/font.h"
#include "font_tpl.h"

static TPLFile fontTPL;

greets_data greets_data_0;

#define TILES_W 32
#define TILES_H 16
#define TILES_FMT GX_TF_IA4

static void
init_tile_shader (void *dummy)
{
  #include "tile-grid.inc"
}

static unsigned char grid_ascii_equiv[] =
  {
    ' ', '!', '?', '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9', 'A', 'B', 'C',
    'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
    'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',
    'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '#',
    '-', ':', '+', '.', ',', '/'
  };

static unsigned char ascii_to_char[256];

/* Hmm, who do we like...
    hitmen
    3ln
    adinpsz
    excess
    asd
    farbrausch
    fairlight
    playpsyco
    !!m
    mercury
    ukscene
    team twiizers
    trsi
    unstable label
    lft
    mfx
    
    tune is: golgot vs simao: tesla! (170bpm)
*/

static unsigned char message[] =
  " TT "
  " HH "
  " EE "
  "    "
  " SS "
  " CC "
  " AA "
  " RR "
  " YY "
  "    "
  " DD "
  " EE "
  " MM "
  " OO "
  "    "
  "G   "
  "R   "
  "E   "
  "E   "
  "T   "
  "S   "
  "    "
  "T   "
  "O   "
  ":   "
  "    "
  "H   "
  "I   "
  "T   "
  "M   "
  "E  #"
  "N  L"
  " A N"
  " D  "
  " I A"
  " N S"
  " P D"
  " S  "
  " Z E"
  "F  X"
  "A  C"
  "R  E"
  "B  S"
  "R  S"
  "A F "
  "U L "
  "S T "
  "C  P"
  "H  L"
  " ! A"
  " ! Y"
  " M P"
  "M  S"
  "E  Y"
  "R  C"
  "C  O"
  "U U "
  "R K "
  "Y S "
  "  C "
  "T E "
  "R N "
  "S E "
  "I  T"
  " U E"
  " N A"
  " S M"
  " T  "
  " A T"
  " B W"
  " L I"
  " E I"
  "   Z"
  " L E"
  " A R"
  " B S"
  " E  "
  " L L"
  "   F"
  "M  T"
  "F   "
  "X   "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "    "
  "   A"
  "   L"
  "   L"
  "    "
  "   C"
  "   O"
  "   D"
  "   E"
  "   ,"
  "    "
  "   G"
  "   F"
  "   X"
  "   +"
  "   M"
  "   O"
  "   D"
  "   E"
  "   L"
  "   S"
  "    "
  "   B"
  "   Y"
  "    "
  "   P"
  "   U"
  "   P"
  "   P"
  "   E"
  "   H"
  "   /"
  "   C"
  "   R"
  "   T"
  "   C";
  
static void
init_ascii_to_char (void)
{
  int i;
  
  memset (ascii_to_char, 255, sizeof (ascii_to_char));
  
  for (i = 0; i < sizeof (grid_ascii_equiv); i++)
    {
      int s_part = i & 7;
      int t_part = (i >> 3) & 7;
      ascii_to_char[grid_ascii_equiv[i]] = s_part * 16 + t_part;
    }
}

static void
put_text (void *tileidx, int row, int startcol, unsigned char *text,
	  unsigned int textlen, int offset, int len)
{
  unsigned char *c = tileidx;
  int i;
  
  for (i = 0; i < len; i++)
    {
      int offset2 = offset + i * 4;
      int idx = tex_index (startcol + i, row, TILES_W, 8);
      c[idx] = (offset2 >= 0 && offset2 < textlen)
	         ? ascii_to_char[text[offset2]] : 0;
    }

  DCFlushRange (c, GX_GetTexBufferSize (TILES_W, TILES_H, TILES_FMT, GX_FALSE,
					0));
}

static void
greets_init_effect (void *params, backbuffer_info *bbuf)
{
  greets_data *gdata = (greets_data *) params;
  
  gdata->world = create_world (0);
  world_set_perspective (gdata->world, 60.0, 1.33f, 10.0f, 300.0f);
  world_set_pos_lookat_up (gdata->world,
			   (guVector) { 0, 0, 50 },
			   (guVector) { 0, 0, 0 },
			   (guVector) { 0, 1, 0 });

  TPL_OpenTPLFromMemory (&fontTPL, (void *) font_tpl, font_tpl_size);
  TPL_GetTexture (&fontTPL, font, &gdata->fontobj);
  GX_InitTexObjFilterMode (&gdata->fontobj, GX_LINEAR, GX_LINEAR);
  GX_InitTexObjWrapMode (&gdata->fontobj, GX_REPEAT, GX_REPEAT);
  
  gdata->tileidx = memalign (32, GX_GetTexBufferSize (TILES_W, TILES_H,
						      TILES_FMT, GX_FALSE, 0));
  GX_InitTexObj (&gdata->tileidxobj, gdata->tileidx, TILES_W, TILES_H,
		 TILES_FMT, GX_REPEAT, GX_REPEAT, GX_FALSE);
  GX_InitTexObjFilterMode (&gdata->tileidxobj, GX_NEAR, GX_NEAR);

  gdata->tile_shader = create_shader (&init_tile_shader, NULL);
  shader_append_texmap (gdata->tile_shader, &gdata->fontobj, GX_TEXMAP0);
  shader_append_texmap (gdata->tile_shader, &gdata->tileidxobj, GX_TEXMAP1);
  shader_append_texcoordgen (gdata->tile_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);
  memset (gdata->tileidx, 0, GX_GetTexBufferSize (TILES_W, TILES_H,
						  TILES_FMT, GX_FALSE, 0));
  init_ascii_to_char ();

  /*for (i = 0; i < 4; i++)
    put_text (gdata->tileidx, i, i, (unsigned char *) "OOOOOOOOOOOOOO");*/
  
  object_loc_initialise (&gdata->greets_loc, GX_PNMTX0);
}

static void
greets_uninit_effect (void *params, backbuffer_info *bbuf)
{
  greets_data *gdata = (greets_data *) params;

  free_world (gdata->world);
  free (gdata->tileidx);
  free_shader (gdata->tile_shader);
}

static void
render_texture (greets_data *gdata, float zpos, float scroll)
{
  world_info *world = gdata->world;
  Mtx mvtmp;
  extern Mtx tube_rotmtx;

  guMtxIdentity (mvtmp);
  guMtxConcat (mvtmp, tube_rotmtx, mvtmp);

  object_set_matrices (&world->scene, &gdata->greets_loc, world->scene.camera,
		       mvtmp, NULL, world->projection, world->projection_type);

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);

  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  GX_SetCullMode (GX_CULL_NONE);

  scroll = scroll * 8.0f;
  scroll = scroll - floorf (scroll);
  scroll = scroll * 1./8;

  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

  GX_Position3f32 (-40, 8, 0);
  GX_TexCoord2f32 (scroll, 0);

  GX_Position3f32 (40, 8, 0);
  GX_TexCoord2f32 (4 + scroll, 0);

  GX_Position3f32 (-40, -8, 0);
  GX_TexCoord2f32 (scroll, 0.5);

  GX_Position3f32 (40, -8, 0);
  GX_TexCoord2f32 (4 + scroll, 0.5);

  GX_End ();

  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

  GX_Position3f32 (-40, 0, 8);
  GX_TexCoord2f32 (scroll, 0);

  GX_Position3f32 (40, 0, 8);
  GX_TexCoord2f32 (4 + scroll, 0);

  GX_Position3f32 (-40, 0, -8);
  GX_TexCoord2f32 (scroll, 0.5);

  GX_Position3f32 (40, 0, -8);
  GX_TexCoord2f32 (4 + scroll, 0.5);

  GX_End ();
}

static void
greets_display_effect (sync_info *sync, void *params, int iparam)
{
  greets_data *gdata = (greets_data *) params;
  f32 indmtx[2][3] = { { 0.5, 0.0, 0.0 },
		       { 0.0, 0.5, 0.0 } };
  float scroll = (float) sync->time_offset / 900.0;
  int i, startpos;

  /*world_set_pos_lookat_up (gdata->world,
			   (guVector) spooky_ghost_data_0.scene.pos,
			   (guVector) spooky_ghost_data_0.scene.lookat,
			   (guVector) spooky_ghost_data_0.scene.up);*/

  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 5);

  world_display (gdata->world);
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  
  shader_load (gdata->tile_shader);

  startpos = (int) (scroll * 8.0) - 32;

  for (i = 0; i < 4; i++)
    put_text (gdata->tileidx, i, 0, message, sizeof (message) - 1,
	      startpos * 4 + i, 32);

  GX_SetTevColor (GX_TEVREG0, (GXColor) { 255, 255, 255, 255 });
  render_texture (gdata, 5.5, scroll);

  //screenspace_rect (gdata->tile_shader, GX_VTXFMT0, 0);
}

effect_methods greets_methods =
{
  .preinit_assets = NULL,
  .init_effect = &greets_init_effect,
  .prepare_frame = NULL,
  .display_effect = &greets_display_effect,
  .uninit_effect = &greets_uninit_effect,
  .finalize = NULL
};
