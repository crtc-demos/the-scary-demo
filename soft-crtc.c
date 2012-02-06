#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "soft-crtc.h"
#include "light.h"
#include "object.h"
#include "server.h"
#include "cam-path.h"

#include "objects/soft-crtc.xyz"

#include "objects/softcube.inc"

INIT_OBJECT (softcube_obj, softcube);

static light_info light0 =
{
  .pos = { 20, 20, -30 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static light_info light1 =
{
  .pos = { -20, -20, -20 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 perspmat;
static scene_info scene =
{
  .pos = { 0, 0, -30 },
  .up = { 0, 1, 0 },
  .lookat = { 0, 0, 0 },
  .camera_dirty = 1
};
static float deg = 0.0;
static float deg2 = 0.0;

static void
specular_lighting_1light (void)
{
  GXLightObj lo0, lo1;
  guVector ldir;

#include "cube-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 16, 32, 16, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 64, 128, 64, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT0 | GX_LIGHT1, GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG,
		  GX_LIGHT0 | GX_LIGHT1, GX_DF_CLAMP, GX_AF_SPEC);

  GX_SetChanCtrl (GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
		  GX_DF_CLAMP, GX_AF_NONE);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  /* Another dimmer, redder light.  */
  guVecSub (&light1.tpos, &light1.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo1, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo1, 32);
  GX_InitLightColor (&lo1, (GXColor) { 160, 64, 64, 192 });
  GX_LoadLightObj (&lo1, GX_LIGHT1);

  GX_InvalidateTexAll ();
}

#define GRID_WIDTH 25
#define GRID_HEIGHT 25

static char *grid[2];
static int current_grid = 0;
static int counter = 0;
static int sometimes = 0;
static int get_bigger = 0;
static float block_size = 0.5;
static float block_size_acc = 0.0;

#define CELL(G,W,H)				\
  ({						\
    int w_wrapped = (W);			\
    int h_wrapped = (H);			\
						\
    if (w_wrapped < 0)				\
      w_wrapped = GRID_WIDTH - 1;		\
    else if (w_wrapped >= GRID_WIDTH)		\
      w_wrapped = 0;				\
						\
    if (h_wrapped < 0)				\
      h_wrapped = GRID_HEIGHT - 1;		\
    else if (h_wrapped >= GRID_HEIGHT)		\
      h_wrapped = 0;				\
						\
    (G)[w_wrapped * GRID_HEIGHT + h_wrapped];	\
  })

#define LCELL(G,W,H) \
  ((G)[(W) * GRID_HEIGHT + (H)])

typedef struct grid_config_tag {
  unsigned int setbits[GRID_HEIGHT];
  struct grid_config_tag *next;
} grid_list;

static grid_list *generation_list = NULL;

static void
pack_grid (grid_list *glist, char *grid)
{
  unsigned int i, j;

  for (j = 0; j < GRID_HEIGHT; j++)
    {
      unsigned int mask = 0;

      for (i = 0; i < GRID_WIDTH; i++)
        {
	  if (CELL (grid, i, j))
	    mask |= 1 << i;
	}

      glist->setbits[j] = mask;
    }
}

static void
unpack_grid (grid_list *glist, char *grid)
{
  unsigned int i, j;

  for (j = 0; j < GRID_HEIGHT; j++)
    {
      unsigned int mask = glist->setbits[j];

      for (i = 0; i < GRID_WIDTH; i++)
        {
	  if (mask & (1 << i))
	    LCELL (grid, i, j) = 1;
	  else
	    LCELL (grid, i, j) = 0;
	}
    }
}

#if 0
static grid_list *
add_grid (char *grid, grid_list *oldhead)
{
  grid_list *newgrid = malloc (sizeof (grid_list));
  
  newgrid->next = oldhead;
  
  pack_grid (newgrid, grid);
  
  return newgrid;
}

static grid_list *
delete_grid (char *grid, grid_list *oldhead)
{
  grid_list *following;

  if (!oldhead)
    return NULL;
  
  following = oldhead->next;

  unpack_grid (oldhead, grid);

  free (oldhead);
  
  return following;
}
#endif

static void
set_grid (int step)
{
  int i, j;
  static const char logo[7][7] =
    { { ' ', '2', '2', ' ', ' ', '3', '3' },
      { '2', ' ', ' ', ' ', '3', ' ', ' ' },
      { '2', '2', '2', ' ', '3', ' ', ' ' },
      { ' ', ' ', ' ', ' ', ' ', ' ', ' ' },
      { '1', '1', '1', ' ', ' ', '0', '0' },
      { ' ', '1', ' ', ' ', '0', ' ', ' ' },
      { ' ', '1', ' ', ' ', '0', '0', '0' } };
  unsigned int lhs = (GRID_WIDTH - 7) / 2;
  unsigned int top = (GRID_HEIGHT - 7) / 2;

  memset (grid[0], 0, GRID_WIDTH * GRID_HEIGHT);

  for (j = 6; j >= 0; j--)
    for (i = 0; i < 7; i++)
      {
        int pixel = logo[j][i];
	int topp = top + j - 16 + (step & 15);
	int topp2 = top + j - 16 + ((step - 8) & 15);
	
	if (step < 16 && topp >= 0 && topp < GRID_HEIGHT)
	  LCELL (grid[0], lhs + 6 - i, topp) |= pixel == '0' ? 1 : 0;
	if (step >= 8 && step < 24 && topp2 >= 0 && topp2 < GRID_HEIGHT)
	  LCELL (grid[0], lhs + 6 - i, topp2) |= pixel == '1' ? 1 : 0;
	if (step >= 16 && step < 32 && topp >= 0 && topp < GRID_HEIGHT)
	  LCELL (grid[0], lhs + 6 - i, topp) |= pixel == '2' ? 1 : 0;
	if (step >= 24 && step < 40 && topp2 >= 0 && topp2 < GRID_HEIGHT)
	  LCELL (grid[0], lhs + 6 - i, topp2) |= pixel == '3' ? 1 : 0;

        if (step >= 16)
	  LCELL (grid[0], lhs + 6 - i, top + j) |= pixel == '0' ? 1 : 0;
	if (step >= 24)
	  LCELL (grid[0], lhs + 6 - i, top + j)
	    |= pixel == '0' || pixel == '1' ? 1 : 0;
	if (step >= 32)
	  LCELL (grid[0], lhs + 6 - i, top + j)
	    |= pixel == '0' || pixel == '1' || pixel == '2' ? 1 : 0;
	if (step >= 40)
	  LCELL (grid[0], lhs + 6 - i, top + j) |= pixel != ' ' ? 1 : 0;
      }
}

static void
soft_crtc_init_effect (void *params, backbuffer_info *bbuf)
{
  guPerspective (perspmat, 60, 1.33f, 1.0f, 300.0f);
  
  grid[0] = malloc (GRID_WIDTH * GRID_HEIGHT);
  grid[1] = malloc (GRID_WIDTH * GRID_HEIGHT);
  
  memset (grid[0], 0, GRID_WIDTH * GRID_HEIGHT);
  memset (grid[1], 0, GRID_WIDTH * GRID_HEIGHT);
  
  set_grid (0);
}

static void
soft_crtc_uninit_effect (void *params, backbuffer_info *bbuf)
{
  free (grid[0]);
  free (grid[1]);
}

static void
do_life_step (unsigned int new_idx)
{
  int i, j;
  int old_idx = 1 - new_idx;
  
  for (i = 0; i < GRID_WIDTH; i++)
    for (j = 0; j < GRID_HEIGHT; j++)
      {
        int neighbours = CELL (grid[old_idx], i - 1, j - 1)
	               + CELL (grid[old_idx], i, j - 1)
		       + CELL (grid[old_idx], i + 1, j - 1)
		       + CELL (grid[old_idx], i - 1, j)
		       + CELL (grid[old_idx], i + 1, j)
		       + CELL (grid[old_idx], i - 1, j + 1)
		       + CELL (grid[old_idx], i, j + 1)
		       + CELL (grid[old_idx], i + 1, j + 1);

        if (CELL (grid[old_idx], i, j))
	  {
	    if (neighbours < 2 || neighbours > 3)
	      LCELL (grid[new_idx], i, j) = 0;
	    else if (neighbours == 2 || neighbours == 3)
	      LCELL (grid[new_idx], i, j) = 1;
	  }
	else
	  {
	    if (neighbours == 3)
	      LCELL (grid[new_idx], i, j) = 1;
	    else
	      LCELL (grid[new_idx], i, j) = 0;
	  }
      }
}

static int last_time_offset;
static int bounce_trigger = 0;
static float uprot = 0.0;

static void
soft_crtc_display_effect (uint32_t time_offset, void *params, int iparam)
{
  Mtx modelView, mvi, mvitmp, rotmtx, rotmtx2;
  guVector axis = {0, 1, 0};
  guVector axis2 = {0, 0, 1};
  int i, j;
  uint32_t tmo_mod;
  Mtx id;

  guMtxScale (id, 2, 2, 2);
  {
    float tx, ty, tz;
    tx = guMtxRowCol (id, 0, 1);
    ty = guMtxRowCol (id, 1, 1);
    tz = guMtxRowCol (id, 2, 1);
    guMtxRowCol (id, 0, 1) = guMtxRowCol (id, 0, 2);
    guMtxRowCol (id, 1, 1) = guMtxRowCol (id, 1, 2);
    guMtxRowCol (id, 2, 1) = guMtxRowCol (id, 2, 2);
    guMtxRowCol (id, 0, 2) = tx;
    guMtxRowCol (id, 1, 2) = ty;
    guMtxRowCol (id, 2, 2) = tz;
  }
  cam_path_follow (&scene, id, &soft_crtc, (float) time_offset / 10000.0);

  scene.up.x = sinf (uprot);
  scene.up.y = cosf (uprot);
  scene.up.z = 0.0;

  uprot -= 0.002;

  scene_update_camera (&scene);

  GX_LoadProjectionMtx (perspmat, GX_PERSPECTIVE);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  light_update (scene.camera, &light0);
  light_update (scene.camera, &light1);

  specular_lighting_1light ();
  
  guMtxIdentity (modelView);
  guMtxRotAxisDeg (rotmtx, &axis, deg);
  guMtxRotAxisDeg (rotmtx2, &axis2, deg2);
  
  object_set_arrays (&softcube_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0,
		     GX_VA_TEX0);

  if (counter < 256)
    set_grid (counter / 4);
  
  tmo_mod = (time_offset / 10) % 35;
  
  if (tmo_mod < last_time_offset)
    bounce_trigger = 1;
  
  last_time_offset = tmo_mod;
  
  /*if (bounce_trigger)
    {
      block_size = 1.0;
      bounce_trigger = 0;
    }
  else
    block_size += (0.5 - block_size) / 20.0;*/
  
  for (i = 0; i < GRID_WIDTH; i++)
    for (j = 0; j < GRID_HEIGHT; j++)
      {
        if (CELL (grid[current_grid], i, j))
	  {
            Mtx mvtmp;
	    float i_f = (float) i - (float) GRID_WIDTH / 2.0;
	    float j_f = (float) j - (float) GRID_HEIGHT / 2.0;

	    guMtxTransApply (modelView, mvtmp, i_f * 2.2, -j_f * 2.2, 0.0);

	    guMtxScaleApply (mvtmp, mvtmp, block_size, block_size, block_size);
	    
	    if (get_bigger)
	      {
	        block_size += block_size_acc;
		block_size_acc += (2.0 - block_size) / 2000.0;
		block_size_acc *= 0.999;
	      }

	    /*guMtxConcat (rotmtx, mvtmp, mvtmp);
	    guMtxConcat (rotmtx2, mvtmp, mvtmp);*/

	    guMtxConcat (scene.camera, mvtmp, mvtmp);

	    GX_LoadPosMtxImm (mvtmp, GX_PNMTX0);
	    guMtxInverse (mvtmp, mvitmp);
	    guMtxTranspose (mvitmp, mvi);
	    GX_LoadNrmMtxImm (mvi, GX_PNMTX0);

	    object_render (&softcube_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);
	  }
      }

  counter++;

  if (counter >= 350)
    {
      current_grid = 1 - current_grid;
      do_life_step (current_grid);
    }

/*
  
  if (counter < 200)
    {
      generation_list = add_grid (grid[current_grid], generation_list);

      current_grid = 1 - current_grid;
      do_life_step (current_grid);
    }
  else
    {
      sometimes++;
      
      if (sometimes > 3)
        {
	  generation_list = delete_grid (grid[current_grid], generation_list);
	  sometimes = 0;
	  
	  if (generation_list == NULL)
	    get_bigger = 1;
	}
    }
  
  deg++;
  deg2 += 0.5;
*/
}

effect_methods soft_crtc_methods =
{
  .preinit_assets = NULL,
  .init_effect = &soft_crtc_init_effect,
  .prepare_frame = NULL,
  .display_effect = &soft_crtc_display_effect,
  .uninit_effect = &soft_crtc_uninit_effect,
  .finalize = NULL
};
