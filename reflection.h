#ifndef REFLECTION_H
#define REFLECTION_H 1

#include "skybox.h"
#include "reduce-cubemap.h"
#include "world.h"

typedef struct
{
  skybox_info *skybox;
  cubemap_info *cubemap;
  shader_info *plain_texture_shader;
  
  world_info *world;

  /* Stuff for ghost (now skull!) object.  */
  object_loc ghost_loc;
  Mtx ghost_mv;
  Mtx ghost_scale;
  shader_info *ghost_shader;
  
  GXTexObj tangentmap;
  GXTexObj aomap;
  
  /* Stuff for rib object.  */
  object_loc rib_loc;
  Mtx rib_mv;
  Mtx rib_scale;
  shader_info *rib_shader;
  
  void *rib_dl;
  u32 rib_dl_size;

  void *rib_lo_dl;
  u32 rib_lo_dl_size;
} reflection_data;

extern effect_methods reflection_methods;
extern reflection_data reflection_data_0;

#endif
