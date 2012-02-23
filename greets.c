#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <stdlib.h>

#include "timing.h"
#include "greets.h"
#include "screenspace.h"

#include "images/font.h"
#include "font_tpl.h"

static TPLFile fontTPL;

greets_data greets_data_0;

#define TILES_W 64
#define TILES_H 32
#define TILES_FMT GX_TF_IA4

static void
init_tile_shader (void *dummy)
{
  #include "tile-grid.inc"
}

static void
put_data (void *tileidx)
{
  char *c = tileidx;
  int i, j;
  
  for (i = 0; i < TILES_W; i++)
    for (j = 0; j < TILES_H; j++)
      {
        unsigned int idx = tex_index (i, j, TILES_W, 8);
	c[idx] = i * 16 + j;
	//c[idx + 1] = rand () & 15;
      }
  DCFlushRange (c, TILES_W * TILES_H * 2);
}

static void
greets_init_effect (void *params, backbuffer_info *bbuf)
{
  greets_data *gdata = (greets_data *) params;
  
  gdata->world = create_world (0);
  world_set_perspective (gdata->world, 60.0, 1.33f, 10.0f, 500.0f);
  world_set_pos_lookat_up (gdata->world,
			   (guVector) { 0, 0, -30 },
			   (guVector) { 0, 0, 0 },
			   (guVector) { 0, 1, 0 });

  TPL_OpenTPLFromMemory (&fontTPL, (void *) font_tpl, font_tpl_size);
  TPL_GetTexture (&fontTPL, font, &gdata->fontobj);
  GX_InitTexObjFilterMode (&gdata->fontobj, GX_NEAR, GX_NEAR);
  GX_InitTexObjWrapMode (&gdata->fontobj, GX_REPEAT, GX_REPEAT);
  
  gdata->tileidx = memalign (32, GX_GetTexBufferSize (TILES_W, TILES_H,
						      TILES_FMT, GX_FALSE, 0));
  GX_InitTexObj (&gdata->tileidxobj, gdata->tileidx, TILES_W, TILES_H,
		 TILES_FMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GX_InitTexObjFilterMode (&gdata->tileidxobj, GX_NEAR, GX_NEAR);

  gdata->tile_shader = create_shader (&init_tile_shader, NULL);
  shader_append_texmap (gdata->tile_shader, &gdata->fontobj, GX_TEXMAP0);
  shader_append_texmap (gdata->tile_shader, &gdata->tileidxobj, GX_TEXMAP1);
  shader_append_texcoordgen (gdata->tile_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);
  put_data (gdata->tileidx);
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
greets_display_effect (uint32_t time_offset, void *params, int iparam)
{
  greets_data *gdata = (greets_data *) params;
  f32 indmtx[2][3] = { { 0.5, 0.0, 0.0 },
		       { 0.0, 0.5, 0.0 } };

  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 5);

  world_display (gdata->world);
  //shader_load (gdata->tile_shader);
  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  screenspace_rect (gdata->tile_shader, GX_VTXFMT0, 0);
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
