#include <ogcsys.h>
#include <gccore.h>
#include <stdlib.h>

#include "shader.h"
#include "object.h"
#include "skybox.h"

#include "images/sky.h"
#include "sky_tpl.h"

static TPLFile skyTPL;

static void
tex_shader_setup (void *data)
{
  skybox_shader_data *shader_data = data;
  int face = shader_data->face;

  #include "skybox.inc"
}

static const int faceno[6] =
{
  GX_TEXMAP0,
  GX_TEXMAP1,
  GX_TEXMAP2,
  GX_TEXMAP3,
  GX_TEXMAP4,
  GX_TEXMAP5
};

skybox_info *
create_skybox (float radius, tev_setup_fn shader, void *private_data)
{
  skybox_info *skybox = malloc (sizeof (skybox_info));
  int i;
  tev_setup_fn use_shader = shader ? shader : &tex_shader_setup;
  
  TPL_OpenTPLFromMemory (&skyTPL, (void *) sky_tpl, sky_tpl_size);
  
  TPL_GetTexture (&skyTPL, sky_left, &skybox->tex[SKYBOX_LEFT]);
  TPL_GetTexture (&skyTPL, sky_front, &skybox->tex[SKYBOX_FRONT]);
  TPL_GetTexture (&skyTPL, sky_right, &skybox->tex[SKYBOX_RIGHT]);
  TPL_GetTexture (&skyTPL, sky_back, &skybox->tex[SKYBOX_BACK]);
  TPL_GetTexture (&skyTPL, sky_top, &skybox->tex[SKYBOX_TOP]);
  TPL_GetTexture (&skyTPL, sky_bottom, &skybox->tex[SKYBOX_BOTTOM]);

  for (i = 0; i < 6; i++)
    GX_InitTexObjWrapMode (&skybox->tex[i], GX_CLAMP, GX_CLAMP);

  for (i = 0; i < 6; i++)
    {
      skybox->shader_data[i].face = i;
      skybox->shader_data[i].private_data = private_data;
      skybox->face_shader[i] = create_shader (use_shader,
					      (void *) &skybox->shader_data[i]);
      shader_append_texmap (skybox->face_shader[i], &skybox->tex[i], faceno[i]);
      shader_append_texcoordgen (skybox->face_shader[i], GX_TEXCOORD0,
				 GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    }

  skybox->radius = radius;

  object_loc_initialise (&skybox->skybox_loc, GX_PNMTX0);

  return skybox;
}

void
free_skybox (skybox_info *skybox)
{
  int i;
  
  for (i = 0; i < 6; i++)
    free_shader (skybox->face_shader[i]);

  free (skybox);
}

void
skybox_set_matrices (scene_info *scene, Mtx camera, skybox_info *skybox,
		     Mtx projection, u32 projection_type)
{
  Mtx offset_mtx;
  Mtx sep_scale;
  
  guMtxTrans (offset_mtx, scene->pos.x / skybox->radius,
	      scene->pos.y / skybox->radius, scene->pos.z / skybox->radius);
  guMtxScale (sep_scale, skybox->radius, skybox->radius, skybox->radius);

  object_set_matrices (scene, &skybox->skybox_loc, camera, offset_mtx,
		       sep_scale, projection, projection_type);
}

/***
    5_______ 7
    /:     /|
  1/______/3|
   | :    | |   +y
   |4:....|.|6  ^  +z
   | ,    | ,   | 7`
   |,_____|/    |/
  0        2    *---> +x

***/

static f32 __attribute__((aligned(32))) vertices[] =
{
  -1, -1, -1,
  -1,  1, -1,
   1, -1, -1,
   1,  1, -1,
  -1, -1,  1,
  -1,  1,  1,
   1, -1,  1,
   1,  1,  1
};

static u8 faces[6][4] =
{
  { 4, 5, 0, 1 },  /* left */
  { 0, 1, 2, 3 },  /* front */
  { 2, 3, 6, 7 },  /* right */
  { 6, 7, 4, 5 },  /* back */
  { 1, 5, 3, 7 },  /* top */
  { 4, 0, 6, 2 }   /* bottom */
};

void
skybox_render (skybox_info *skybox)
{
  const u32 vtxfmt = GX_VTXFMT0;
  int face;

  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_INDEX8);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  
  GX_SetArray (GX_VA_POS, vertices, 3 * sizeof (f32));
  
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  
  GX_SetCullMode (GX_CULL_BACK);
  
  for (face = 0; face < 6; face++)
    {
      shader_load (skybox->face_shader[face]);

      GX_Begin (GX_TRIANGLESTRIP, vtxfmt, 4);

      GX_Position1x8 (faces[face][0]);
      GX_TexCoord2f32 (0, 1);

      GX_Position1x8 (faces[face][1]);
      GX_TexCoord2f32 (0, 0);

      GX_Position1x8 (faces[face][2]);
      GX_TexCoord2f32 (1, 1);

      GX_Position1x8 (faces[face][3]);
      GX_TexCoord2f32 (1, 0);

      GX_End ();
    }
}
