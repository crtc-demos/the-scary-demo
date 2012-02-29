#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "timing.h"
#include "rendertarget.h"
#include "spooky-ghost.h"
#include "light.h"
#include "object.h"
#include "scene.h"
#include "server.h"
#include "ghost-obj.h"
#include "lighting-texture.h"
#include "screenspace.h"
#include "cam-path.h"
#include "matrixutil.h"

#include "objects/tunnel-track.xyz"

#include "images/more_stones.h"
#include "more_stones_tpl.h"

TPLFile stone_textureTPL;

#include "images/stones_bump.h"
#include "stones_bump_tpl.h"

static TPLFile stone_bumpTPL;

#include "objects/tunnel-section.inc"

INIT_OBJECT (tunnel_section_obj, tunnel_section);

#define REFLECTION_W 640
#define REFLECTION_H 480
#define REFLECTION_TEXFMT GX_TF_RGB565

#define TUNNEL_SEPARATION 60

const static int rearrange[] = { 0, -1, 1, -2, 2 };

#undef SEE_TEXTURES

static light_info light0 =
{
  .pos = { 0, 0, -50 },
  .lookat = { 0, 0, 0 },
  .up = { 0, 1, 0 }
};

static Mtx44 perspmat;

spooky_ghost_data spooky_ghost_data_0;

int switch_ghost_lighting = 1;

static void
tunnel_lighting (void *dummy)
{
  GXLightObj lo0;
  guVector ldir;

#include "tunnel-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, (GXColor) { 224, 224, 224, 0 });
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);

  //GX_SetTevKColor (0, (GXColor) { 255, 192, 0, 0 });
}

static void
ghastly_lighting (void *dummy)
{
  GXLightObj lo0;
  guVector ldir;
  GXColor matcolour = *(GXColor *) dummy;

#include "ghastly-lighting.inc"

  GX_SetChanAmbColor (GX_COLOR0, (GXColor) { 32, 32, 32, 0 });
  GX_SetChanMatColor (GX_COLOR0, matcolour);
  GX_SetChanCtrl (GX_COLOR0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_CLAMP, GX_AF_NONE);

  GX_SetChanAmbColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 0 });
  GX_SetChanMatColor (GX_ALPHA0, (GXColor) { 0, 0, 0, 255 });
  GX_SetChanCtrl (GX_ALPHA0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_SPEC);

  /* Light 0: use for both specular and diffuse lighting.  */
  guVecSub (&light0.tpos, &light0.tlookat, &ldir);
  guVecNormalize (&ldir);

  GX_InitSpecularDir (&lo0, -ldir.x, -ldir.y, -ldir.z);
  GX_InitLightShininess (&lo0, 64);
  GX_InitLightColor (&lo0, (GXColor) { 192, 192, 192, 255 });
  GX_LoadLightObj (&lo0, GX_LIGHT0);
}

static void
bump_mapping_tev_setup (void *dummy)
{
  f32 indmtx[2][3] = { { 0.5, 0.0, 0.0 },
		       { 0.0, 0.5, 0.0 } };
#include "bump-mapping.inc"
  //GX_SetIndTexCoordScale (GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
  GX_SetIndTexMatrix (GX_ITM_0, indmtx, 4);
  
  GX_SetChanCtrl (GX_COLOR0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_NONE);
  GX_SetChanCtrl (GX_ALPHA0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0,
		  GX_DF_NONE, GX_AF_NONE);
}

static void
water_setup (void *dummy)
{
  #include "water-texture.inc"
}

static void
spooky_ghost_init_effect (void *params, backbuffer_info *bbuf)
{
  spooky_ghost_data *sdata = (spooky_ghost_data *) params;

  guPerspective (perspmat, 60, 1.33f, 5.0f, 500.0f);
  
  scene_set_pos (&sdata->scene, (guVector) { 0, 0, 30 });
  scene_set_up (&sdata->scene, (guVector) { 0, 1, 0 });
  scene_set_lookat (&sdata->scene, (guVector) { 0, 0, 0 });
  
  scene_update_camera (&sdata->scene);
  
  object_loc_initialise (&sdata->tunnel_section_loc, GX_PNMTX0);
  object_set_tex_norm_binorm_matrices (&sdata->tunnel_section_loc,
				       GX_TEXMTX1, GX_TEXMTX2);
  
  object_loc_initialise (&sdata->ghost_loc, GX_PNMTX0);
  
  TPL_OpenTPLFromMemory (&stone_textureTPL, (void *) more_stones_tpl,
			 more_stones_tpl_size);
  TPL_OpenTPLFromMemory (&stone_bumpTPL, (void *) stones_bump_tpl,
			 stones_bump_tpl_size);

  sdata->lighting_texture = create_lighting_texture ();

  sdata->reflection = memalign (32, GX_GetTexBufferSize (REFLECTION_W,
			 REFLECTION_H, REFLECTION_TEXFMT, GX_FALSE, 0));

  /* Set up bump mapping shader.  */
  TPL_GetTexture (&stone_textureTPL, stone_texture, &sdata->texture);
  TPL_GetTexture (&stone_bumpTPL, stone_bump, &sdata->bumpmap);

  sdata->bump_mapping_shader = create_shader (&bump_mapping_tev_setup, NULL);
  shader_append_texmap (sdata->bump_mapping_shader, &sdata->texture,
			GX_TEXMAP0);
  shader_append_texmap (sdata->bump_mapping_shader,
			&sdata->lighting_texture->texobj, GX_TEXMAP1);
  shader_append_texmap (sdata->bump_mapping_shader, &sdata->bumpmap,
			GX_TEXMAP2);
  shader_append_texcoordgen (sdata->bump_mapping_shader, GX_TEXCOORD0,
			     GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  shader_append_texcoordgen (sdata->bump_mapping_shader, GX_TEXCOORD1,
			     GX_TG_MTX2x4, GX_TG_BINRM, GX_TEXMTX2);
  shader_append_texcoordgen (sdata->bump_mapping_shader, GX_TEXCOORD2,
			     GX_TG_MTX2x4, GX_TG_TANGENT, GX_TEXMTX2);
  shader_append_texcoordgen (sdata->bump_mapping_shader, GX_TEXCOORD3,
			     GX_TG_MTX2x4, GX_TG_NRM, GX_TEXMTX1);

  /* Plain lighting.  */
  sdata->tunnel_lighting_shader = create_shader (&tunnel_lighting, NULL);
  
  /* Water shader.  */
  GX_InitTexObj (&sdata->reflection_obj, sdata->reflection, REFLECTION_W,
		 REFLECTION_H, REFLECTION_TEXFMT, GX_CLAMP, GX_CLAMP, GX_FALSE);
  sdata->water_shader = create_shader (&water_setup, NULL);
  shader_append_texmap (sdata->water_shader, &sdata->reflection_obj,
			GX_TEXMAP3);
  shader_append_texcoordgen (sdata->water_shader, GX_TEXCOORD0, GX_TG_MTX2x4,
			     GX_TG_TEX0, GX_IDENTITY);

  /* Ghost shader.  */
  sdata->ghost_shader = create_shader (&ghastly_lighting, &sdata->ghost.colour);
  sdata->ghost.visible = false;
  sdata->ghost.number = -1;
}

static void
spooky_ghost_uninit_effect (void *params, backbuffer_info *bbuf)
{
  spooky_ghost_data *sdata = (spooky_ghost_data *) params;

  free_lighting_texture (sdata->lighting_texture);
  free_shader (sdata->bump_mapping_shader);
  free_shader (sdata->tunnel_lighting_shader);
  free_shader (sdata->water_shader);
  free (sdata->reflection);
}

#if 0
#define WAVES 5

static float offset[5][64][64];

static float
height_at (float x, float y, float phase)
{
  int i, x, y;
  float ht = 0.0;
  static int initialised = 0;
  
  if (!initialised)
    {
      for (i = 0; i < WAVES; i++)
        {
	  int x_divisions = (lrand48 () % 16) + 8;
	  int y_divisions = (lrand48 () % 16) + 8;
	  float x_amp = drand48 ();
	  float y_amp = drand48 ();

	  for (x = 0; x < 64; x++)
	    {
	      for (y = 0; y < 64; y++)
	        {
		  offset[i][x][y] = x_amp * sin (x * 2 * M_PI
				    / (64.0 * x_divisions));
		  offset[i][x][y] += y_amp * sin (y * 2 * M_PI
				     / (64.0 * y_divisions));
		}
	    }
	}
      initialised = 1;
    }
  
  for (i = 0; i < WAVES; i++)
    {
      ht += x_amp[i] * sinf (phase * x_phase[i] + x_freq[i] * x);
      ht += y_amp[i] * sinf (phase * y_phase[i] + y_freq[i] * y);
    }
  
  return ht;
}

float w_phase = 0;

static void
draw_waves (void)
{
  u32 i, j;
  guVector c, cn;
  const u32 isize = 64, jsize = 64;

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (GX_VTXFMT1, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);

  GX_SetCullMode (GX_CULL_NONE);

  for (i = 0; i < isize; i++)
    {
      GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT1, jsize * 2 + 2);
      
      for (j = 0; j <= jsize; j++)
        {
	  float xf = (2.0 * (float) i / isize) - 1.0;
	  float yf = (2.0 * (float) j / jsize) - 1.0;
	  c.x = 100 + 100 * xf;
	  c.y = -15 + height_at (xf, yf, w_phase);
	  c.z = 100 * yf;
	  cn.x = 0;
	  cn.y = 1;
	  cn.z = 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	  
	  xf = (2.0 * (float) (i + 1) / isize) - 1.0;
	  
	  c.x = 100 + 100 * xf;
	  c.y = -15 + height_at (xf, yf, w_phase);
	  cn.x = 0;
	  cn.y = 1;
	  cn.z = 0;
	  GX_Position3f32 (c.x, c.y, c.z);
	  GX_Normal3f32 (cn.x, cn.y, cn.z);
	}
      
      GX_End ();
    }
  w_phase += 0.01;
}
#endif

static void
render_tunnel (spooky_ghost_data *sdata, bool reflect, Mtx extra_xform,
	       float camera_pos)
{
  int i;
  const float fudge_factor = 1.15;
  const float size = 28.0;
  Mtx mvtmp, sep_scale;
  float block_start = camera_pos / (fudge_factor * size);
  
  block_start = floorf (block_start) * fudge_factor * size;
  
  if (reflect)
    {
      GX_SetCullMode (GX_CULL_FRONT);
      guMtxScale (sep_scale, size, -size, size);
    }
  else
    {
      GX_SetCullMode (GX_CULL_BACK);
      guMtxScale (sep_scale, size, size, size);
    }
  
  object_set_arrays (&tunnel_section_obj,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD, GX_VTXFMT0,
		     GX_VA_TEX0);
  
  for (i = 0; i < 15; i++)
    {
      if (reflect)
	guMtxTrans (mvtmp, block_start + i * fudge_factor * size,
			   -1.15 * size, 0.0);
      else
	guMtxTrans (mvtmp, block_start + i * fudge_factor * size, 0, 0.0);
      
      guMtxConcat (extra_xform, mvtmp, mvtmp);
      
      object_set_matrices (&sdata->scene, &sdata->tunnel_section_loc,
			   sdata->scene.camera, mvtmp, sep_scale, NULL, 0);
      
      object_render (&tunnel_section_obj,
		     OBJECT_POS | OBJECT_NBT3 | OBJECT_TEXCOORD, GX_VTXFMT0);
    }
}


static void
place_ghost (spooky_ghost_data *sdata, int blocknum, GXColor colour,
	     ghost_dir ghost_dir)
{
  switch (ghost_dir)
    {
    case GHOST_RIGHT:
    case GHOST_LEFT:
      sdata->ghost.acrossness = (ghost_dir == GHOST_RIGHT) ? -18.0 : 18.0;
      break;
    
    case GHOST_TOWARDS:
      sdata->ghost.acrossness = 0;
      break;
    }
  sdata->ghost.alongness = blocknum * 28.0 * 1.15;
  sdata->ghost.colour = colour;
  sdata->ghost.visible = true;
  sdata->ghost.ghost_dir = ghost_dir;
  sdata->ghost.number++;
}

static display_target
spooky_ghost_prepare_frame (uint32_t time_offset, void *params, int iparam)
{
  spooky_ghost_data *sdata = (spooky_ghost_data *) params;
  Mtx modelView, mvtmp, sep_scale, rot, id, exfm;
  int i;

  light0.pos.x = cos (sdata->lightdeg) * 500.0;
  light0.pos.y = -1000; // sin (lightdeg / 1.33) * 300.0;
  light0.pos.z = sin (sdata->lightdeg) * 500.0;
  
  sdata->lightdeg += 0.03;

  if (switch_ghost_lighting)
    {
      shader_load (sdata->tunnel_lighting_shader);
      light_update (sdata->scene.camera, &light0);
      update_lighting_texture (&sdata->scene, sdata->lighting_texture);
    }

#if 1
  guMtxScale (id, 28, 28, 28);
  matrixutil_swap_yz (id, id);
  cam_path_follow (&sdata->scene, id, &tunnel_track,
		   (float) time_offset / 60000.0);
  sdata->bla = sdata->scene.pos.x - 28.0;
#else
  scene_set_pos (&sdata->scene, (guVector) { sdata->bla, 0.0, 0.0 });
  scene_set_lookat (&sdata->scene, (guVector)
		    { sdata->bla + 5,
		      cos (sdata->deg) * 0.2 + cos (sdata->deg2) * 0.1,
		      sin (sdata->deg2) * 0.2 + sin (sdata->deg) * 0.1 });
#endif

  //guLookAt (viewmat, &pos, &up, &lookat);

  GX_InvalidateTexAll ();
  
  scene_update_camera (&sdata->scene);

  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
  GX_SetBlendMode (GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_SET);
  GX_SetColorUpdate (GX_TRUE);
  GX_SetAlphaUpdate (GX_TRUE);

  guMtxIdentity (modelView);

  guMtxScaleApply (modelView, mvtmp, 2.0, 2.0, 2.0);

  light_update (sdata->scene.camera, &light0);

  guMtxIdentity (modelView);
  guMtxScale (sep_scale, 25.0, 25.0, 25.0);
  object_set_matrices (&sdata->scene, &sdata->tunnel_section_loc,
		       sdata->scene.camera, modelView, sep_scale, perspmat,
		       GX_PERSPECTIVE);

  light_update (sdata->scene.camera, &light0);

  if (switch_ghost_lighting)
    shader_load (sdata->bump_mapping_shader);
  else
    shader_load (sdata->tunnel_lighting_shader);
    
  GX_SetCurrentMtx (GX_PNMTX0);
  
  rendertarget_texture (REFLECTION_W, REFLECTION_H, REFLECTION_TEXFMT,
			GX_FALSE, GX_PF_RGB8_Z24, GX_ZC_LINEAR);

  for (i = -2; i <= 2; i++)
    {
      guMtxTrans (exfm, 0, 0, (float) rearrange[i] * TUNNEL_SEPARATION);
      render_tunnel (sdata, true, exfm, sdata->bla);
    }

  if (sdata->ghost.number == -1 && time_offset > 4000)
    place_ghost (sdata, 2, (GXColor) { 255, 32, 32, 0 }, GHOST_LEFT);
  else if (sdata->ghost.number == 0 && time_offset > 9000)
    place_ghost (sdata, 5, (GXColor) { 255, 32, 32, 0 }, GHOST_RIGHT);
  else if (sdata->ghost.number == 1 && time_offset > 14000)
    {
      sdata->ghost.visible = false;
      if (time_offset > 25000)
	place_ghost (sdata, 9, (GXColor) { 255, 255, 32, 0 }, GHOST_RIGHT);
    }
  else if (sdata->ghost.number == 2 && time_offset > 32000)
    place_ghost (sdata, 12, (GXColor) { 255, 32, 32, 0 }, GHOST_LEFT);
  else if (sdata->ghost.number == 3 && time_offset > 47000)
    place_ghost (sdata, 20, (GXColor) { 255, 192, 32, 0 }, GHOST_TOWARDS);

  /* Ghost.  */
  if (sdata->ghost.visible)
    {
      GX_SetCullMode (GX_CULL_FRONT);
      object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM,
			 GX_VTXFMT0, 0);

      guMtxTrans (mvtmp, sdata->ghost.alongness, -16, sdata->ghost.acrossness);
      guMtxScale (sep_scale, 5.0, -5.0, 5.0);
      switch (sdata->ghost.ghost_dir)
        {
	case GHOST_LEFT:
	  guMtxRotRad (rot, 'y', M_PI);
	  guMtxConcat (mvtmp, rot, mvtmp);
	  break;
	  
	case GHOST_RIGHT:
	  break;
	
	case GHOST_TOWARDS:
	  guMtxRotRad (rot, 'y', -M_PI / 2.0);
	  guMtxConcat (mvtmp, rot, mvtmp);
	  break;
	}

      object_set_matrices (&sdata->scene, &sdata->ghost_loc,
			   sdata->scene.camera, mvtmp, sep_scale, NULL, 0);


      /*sdata->ghost.colour = (GXColor) { 255, 32, 32 };*/
      shader_load (sdata->ghost_shader);

      object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);
    }

  GX_CopyTex (sdata->reflection, GX_TRUE);
  GX_PixModeSync ();

  return MAIN_BUFFER;
}

#ifdef SEE_TEXTURES

static void
draw_square (void)
{
  GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

  GX_Position3f32 (1, -1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (1, 0);

  GX_Position3f32 (-1, -1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (0, 0);

  GX_Position3f32 (1, 1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (1, 1);

  GX_Position3f32 (-1, 1, 0);
  GX_Normal3f32 (0, 0, -1);
  GX_TexCoord2f32 (0, 1);

  GX_End ();
}

#endif

#ifdef SEE_TEXTURES
void dummy ()
{
  /* Draw an square in space.  */
  {
    Mtx mvtmpx;
    
    guMtxTransApply (mvtmp, mvtmpx, -5.0, 12.0, 0.0);
    update_matrices (mvtmpx, viewmat);
    
    GX_SetTexCoordGen (GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_ClearVtxDesc ();
    GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc (GX_VA_NRM, GX_DIRECT);
    GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt (GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    
    #include "just-texture.inc"
    
    draw_square ();

    #include "alpha-texture.inc"

    guMtxTransApply (mvtmp, mvtmpx, 5.0, 12.0, 0.0);
    update_matrices (mvtmpx, viewmat);
    draw_square ();
  }
}
#endif


static void
spooky_ghost_display_effect (uint32_t time_offset, void *params, int iparam)
{
  spooky_ghost_data *sdata = (spooky_ghost_data *) params;
  Mtx modelView, mvtmp, sep_scale, rot, exfm;
  int i;

  /* Draw "water".  */
#if 1
  GX_SetZMode (GX_FALSE, GX_LEQUAL, GX_FALSE);
  screenspace_rect (sdata->water_shader, GX_VTXFMT1, 0);
  GX_SetZMode (GX_TRUE, GX_LEQUAL, GX_TRUE);
#else
  guMtxIdentity (modelView);
  object_set_matrices (&scene, &sdata->tunnel_section_loc, scene.camera,
		       modelView);
  shader_load (sdata->tunnel_lighting_shader);
  draw_waves ();
#endif

  guMtxIdentity (modelView);
  object_set_matrices (&sdata->scene, &sdata->tunnel_section_loc,
		       sdata->scene.camera, modelView, sep_scale, perspmat,
		       GX_PERSPECTIVE);

  if (switch_ghost_lighting)
    shader_load (sdata->bump_mapping_shader);
  else
    shader_load (sdata->tunnel_lighting_shader);

  GX_SetFog (GX_FOG_PERSP_LIN, 50, 300, 10.0, 500.0, (GXColor) { 0, 0, 0 });

  for (i = -2; i <= 2; i++)
    {
      guMtxTrans (exfm, 0, 0, (float) rearrange[i] * TUNNEL_SEPARATION);
      render_tunnel (sdata, false, exfm, sdata->bla);
    }

  /*sdata->bla += 0.15;*/
  /*if (sdata->bla >= size * fudge_factor)
    sdata->bla -= size * fudge_factor;*/

  /* Ghost.  */
  if (sdata->ghost.visible)
    {
      GX_SetCullMode (GX_CULL_BACK);
      object_set_arrays (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM,
			 GX_VTXFMT0, 0);

      guMtxIdentity (modelView);

      guMtxTrans (mvtmp, sdata->ghost.alongness, -14, sdata->ghost.acrossness);
      guMtxScale (sep_scale, 5.0, 5.0, 5.0);
      switch (sdata->ghost.ghost_dir)
        {
	case GHOST_LEFT:
	  guMtxRotRad (rot, 'y', M_PI);
	  guMtxConcat (mvtmp, rot, mvtmp);
	  break;
	  
	case GHOST_RIGHT:
	  break;
	
	case GHOST_TOWARDS:
	  guMtxRotRad (rot, 'y', -M_PI / 2.0);
	  guMtxConcat (mvtmp, rot, mvtmp);
	  break;
	}

      object_set_matrices (&sdata->scene, &sdata->ghost_loc,
			   sdata->scene.camera, mvtmp, sep_scale, NULL, 0);

      shader_load (sdata->ghost_shader);

      object_render (&spooky_ghost_obj, OBJECT_POS | OBJECT_NORM, GX_VTXFMT0);
    }

  sdata->deg += 0.012;
  sdata->deg2 += 0.05;
  sdata->ghost.sometimes++;
  if (sdata->ghost.sometimes > 5)
    {
      switch (sdata->ghost.ghost_dir)
        {
	case GHOST_RIGHT:
	  sdata->ghost.acrossness += 0.8;
	  break;
	
	case GHOST_LEFT:
	  sdata->ghost.acrossness -= 0.8;
	  break;
	
	case GHOST_TOWARDS:
	  sdata->ghost.acrossness = 0.0;
	  sdata->ghost.alongness -= 4.5;
	  break;
	}

      sdata->ghost.sometimes = 0;
    }
}

effect_methods spooky_ghost_methods =
{
  .preinit_assets = NULL,
  .init_effect = &spooky_ghost_init_effect,
  .prepare_frame = &spooky_ghost_prepare_frame,
  .display_effect = &spooky_ghost_display_effect,
  .uninit_effect = &spooky_ghost_uninit_effect,
  .finalize = NULL
};
