#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "shader.h"
#include "server.h"

shader_info *
create_shader (tev_setup_fn setup_tev, void *private_data)
{
  shader_info *shinf = malloc (sizeof (shader_info));
  
  memset (shinf, 0, sizeof (shader_info));
  
  shinf->setup_tev = setup_tev;
  shinf->tev_private_data = private_data;
  
  return shinf;
}

void
free_shader (shader_info *shader)
{
  free (shader);
}

void
shader_append_texmap (shader_info *shinf, GXTexObj *obj, u8 mapid)
{
  assert (shinf->num_textures < SHADER_MAX_TEXTURES);

  shinf->textures[shinf->num_textures].obj = obj;
  shinf->textures[shinf->num_textures].mapid = mapid;
  shinf->textures[shinf->num_textures].preload = false;
  shinf->textures[shinf->num_textures].texregion = NULL;
  
  shinf->num_textures++;
}

void
shader_append_preloaded_texmap (shader_info *shinf, GXTexObj *obj, u8 mapid,
				GXTexRegion *texregion)
{
  assert (shinf->num_textures < SHADER_MAX_TEXTURES);

  shinf->textures[shinf->num_textures].obj = obj;
  shinf->textures[shinf->num_textures].mapid = mapid;
  shinf->textures[shinf->num_textures].preload = true;
  shinf->textures[shinf->num_textures].texregion = texregion;
  
  shinf->num_textures++;
}

void
shader_append_texcoordgen (shader_info *shinf, u16 texcoord, u32 tgen_typ,
			   u32 tgen_src, u32 mtxsrc)
{
  assert (shinf->num_texcoords < SHADER_MAX_TEXCOORDS);
  
  shinf->texcoords[shinf->num_texcoords].texcoord = texcoord;
  shinf->texcoords[shinf->num_texcoords].tgen_typ = tgen_typ;
  shinf->texcoords[shinf->num_texcoords].tgen_src = tgen_src;
  shinf->texcoords[shinf->num_texcoords].mtxsrc = mtxsrc;
  
  shinf->num_texcoords++;
}

void
shader_load (shader_info *shinf)
{
  unsigned int i;
  
  for (i = 0; i < shinf->num_textures; i++)
    {
      if (shinf->textures[i].preload)
	GX_LoadTexObjPreloaded (shinf->textures[i].obj,
				shinf->textures[i].texregion,
				shinf->textures[i].mapid);
      else
        GX_LoadTexObj (shinf->textures[i].obj, shinf->textures[i].mapid);
    }
  
  for (i = 0; i < shinf->num_texcoords; i++)
    GX_SetTexCoordGen (shinf->texcoords[i].texcoord,
		       shinf->texcoords[i].tgen_typ,
		       shinf->texcoords[i].tgen_src,
		       shinf->texcoords[i].mtxsrc);

  shinf->setup_tev (shinf->tev_private_data);
}
