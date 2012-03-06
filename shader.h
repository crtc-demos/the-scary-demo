#ifndef SHADER_H
#define SHADER_H 1

#include <ogcsys.h>
#include <gccore.h>

#define SHADER_MAX_TEXTURES 8
#define SHADER_MAX_TEXCOORDS 8

typedef void (*tev_setup_fn) (void *);

typedef struct
{
  tev_setup_fn setup_tev;
  void *tev_private_data;
  
  struct
  {
    GXTexObj *obj;
    u8 mapid;
    bool preload;
    GXTexRegion *texregion;
  } textures[SHADER_MAX_TEXTURES];

  unsigned int num_textures;
  
  struct
  {
    u16 texcoord;
    u32 tgen_typ;
    u32 tgen_src;
    u32 mtxsrc;
  } texcoords[SHADER_MAX_TEXCOORDS];
  unsigned int num_texcoords;
} shader_info;

extern shader_info *create_shader (tev_setup_fn setup_tev, void *private_data);
extern void free_shader (shader_info *shader);
extern void shader_append_texmap (shader_info *shinf, GXTexObj *obj, u8 mapid);
extern void shader_append_preloaded_texmap (shader_info *shinf, GXTexObj *obj,
					    u8 mapid, GXTexRegion *texregion);
extern void shader_append_texcoordgen (shader_info *shinf, u16 texcoord,
				       u32 tgen_typ, u32 tgen_src, u32 mtxsrc);
extern void shader_load (shader_info *shinf);

#endif
