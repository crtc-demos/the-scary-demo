#include <ogcsys.h>
#include <gccore.h>
#include <malloc.h>
#include <math.h>
#include <assert.h>

#include "shader.h"
#include "object.h"
#include "scene.h"
#include "skybox.h"
#include "reduce-cubemap.h"
#include "rendertarget.h"
#include "server.h"

static void
cubemap_shader_setup (void *dummy)
{
  int face = *(int *) dummy;
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

cubemap_info *
create_cubemap (u32 size, u32 fmt, u32 spsize, u32 spfmt)
{
  cubemap_info *cubemap = malloc (sizeof (cubemap_info));
  int i;
  
  for (i = 0; i < 6; i++)
    {
      cubemap->texels[i] = memalign (32, GX_GetTexBufferSize (size, size, fmt,
				     GX_FALSE, 0));
      GX_InitTexObj (&cubemap->tex[i], cubemap->texels[i], size, size, fmt,
		     GX_CLAMP, GX_CLAMP, GX_FALSE);
      cubemap->face_shader[i] = create_shader (&cubemap_shader_setup,
					       (void *) &faceno[i]);
      shader_append_texmap (cubemap->face_shader[i], &cubemap->tex[i],
			    faceno[i]);
      shader_append_texcoordgen (cubemap->face_shader[i], GX_TEXCOORD0,
				 GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    }

  cubemap->size = size;
  cubemap->fmt = fmt;

  cubemap->sphtexels = memalign (32, GX_GetTexBufferSize (spsize, spsize, spfmt,
				 GX_FALSE, 0));
  GX_InitTexObj (&cubemap->spheretex, cubemap->sphtexels, spsize, spsize, spfmt,
		 GX_CLAMP, GX_CLAMP, GX_FALSE);
  cubemap->sphsize = spsize;
  cubemap->sphfmt = spfmt;

  return cubemap;
}

void
free_cubemap (cubemap_info *cubemap)
{
  int i;
  
  for (i = 0; i < 6; i++)
    {
      free (cubemap->texels[i]);
      free_shader (cubemap->face_shader[i]);
    }

  free (cubemap->sphtexels);
  free (cubemap);
}

/***

   s \          ,r
      \        ,
       \      ,
      	\    ,
      	 \  , 
          \,___________\ -z
                       /
***/

static void
face_to_sphere (guVector *out, int face, float a, float b)
{
  guVector sphere;
  guVector eye = { 0, 0, -1 };

  switch (face)
    {
    case 0:  /* left */
      sphere.x = -1.0;
      sphere.y = -b;
      sphere.z = -a;
      break;

    case 1:  /* front */
      sphere.x = -a;
      sphere.y = -b;
      sphere.z = 1.0;
      break;

    case 2:  /* right */
      sphere.x = 1.0;
      sphere.y = -b;
      sphere.z = a;
      break;

    case 3:  /* back */
      sphere.x = a;
      sphere.y = -b;
      sphere.z = -1;
      break;

    case 4:  /* top */
      sphere.x = -a;
      sphere.y = 1.0;
      sphere.z = b;
      break;

    case 5:  /* bottom */
      sphere.x = -a;
      sphere.y = -1.0;
      sphere.z = -b;
      break;
    }

  /* We just need to add the "sphere" & eye vectors, then normalize.  */

  guVecNormalize (&sphere);
  guVecAdd (&sphere, &eye, out);

  if (out->x == 0 && out->y == 0 && out->z == 0)
    out->z += 0.0001;

  guVecNormalize (out);
}

void
cubemap_cam_matrix_for_face (Mtx cam, scene_info *scene, int face)
{
  Mtx rot;
  /*guPerspective (proj, 90, 1.0f, near, far);*/

  scene_update_camera (scene);
  
  switch (face)
    {
    case 0:  /* left */
      guMtxRotRad (rot, 'y', M_PI / 2);
      guMtxConcat (rot, scene->camera, cam);
      break;

    case 1:  /* front */
      guMtxCopy (scene->camera, cam);
      break;

    case 2:  /* right */
      guMtxRotRad (rot, 'y', -M_PI / 2);
      guMtxConcat (rot, scene->camera, cam);
      break;

    case 3:  /* back */
      guMtxRotRad (rot, 'y', M_PI);
      guMtxConcat (rot, scene->camera, cam);
      break;

    case 4:  /* top */
      guMtxRotRad (rot, 'x', -M_PI / 2);
      guMtxConcat (rot, scene->camera, cam);
      break;

    case 5:  /* bottom */
      guMtxRotRad (rot, 'x', M_PI / 2);
      guMtxConcat (rot, scene->camera, cam);
      break;
    }
}

/* Take a cubemap representing a scene from a given viewpoint, and render it
   down to a spherical environment map.  Clobbers hw matrices and vertex format
   setup.  */

void
reduce_cubemap (cubemap_info *cubemap, int subdiv)
{
  int x, y, face;
  const u32 vtxfmt = GX_VTXFMT0;
  Mtx mvtmp;
  Mtx44 ortho;
  object_loc rect_loc;
  scene_info rect_scene;
  int half_subdiv = subdiv / 2;
  float scale_factor = 1.0 / ((float) half_subdiv - 0.5);
  
  /* Using an even number of subdivisions and spreading evenly from -1...1
     ensures we don't try to evaluate the singularity point at the back of the
     sphere.  */
  assert ((subdiv & 1) == 0);
  
  scene_set_pos (&rect_scene, (guVector) { 0, 0, -5 });
  scene_set_lookat (&rect_scene, (guVector) { 0, 0, 0 });
  scene_set_up (&rect_scene, (guVector) { 0, 1, 0 });
  
  scene_update_camera (&rect_scene);
  
  guOrtho (ortho, -1, 1, -1, 1, 1, 15);
  
  object_loc_initialise (&rect_loc, GX_PNMTX0);
  
  guMtxIdentity (mvtmp);
  
  object_set_matrices (&rect_scene, &rect_loc, rect_scene.camera, mvtmp, NULL,
		       ortho, GX_ORTHOGRAPHIC);
  
  GX_ClearVtxDesc ();
  GX_SetVtxDesc (GX_VA_POS, GX_DIRECT);
  GX_SetVtxDesc (GX_VA_TEX0, GX_DIRECT);
  
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GX_SetVtxAttrFmt (vtxfmt, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  
  GX_SetCullMode (GX_CULL_BACK);
  
  rendertarget_texture (cubemap->sphsize, cubemap->sphsize, cubemap->sphfmt,
			GX_FALSE, GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  
  for (face = 0; face < 6; face++)
    {
      shader_load (cubemap->face_shader[face]);

      for (x = -half_subdiv; x < half_subdiv - 1; x++)
	{
          float a = ((float) x + 0.5) * scale_factor;
          float a1 = ((float) x + 1.5) * scale_factor;

	  GX_Begin (GX_TRIANGLESTRIP, vtxfmt, subdiv * 2);

	  for (y = -half_subdiv; y < half_subdiv; y++)
	    {
	      float b = ((float) y + 0.5) * scale_factor;
	      guVector tmp;

	      face_to_sphere (&tmp, face, a, b);
	      GX_Position3f32 (tmp.x, tmp.y, tmp.z);
	      GX_TexCoord2f32 ((a + 1.0) / 2.0, (b + 1.0) / 2.0);

	      face_to_sphere (&tmp, face, a1, b);
	      GX_Position3f32 (tmp.x, tmp.y, tmp.z);
	      GX_TexCoord2f32 ((a1 + 1.0) / 2.0, (b + 1.0) / 2.0);
	    }

	  GX_End ();
	}
    }
  
  GX_CopyTex (cubemap->sphtexels, GX_TRUE);
  GX_PixModeSync ();
}
