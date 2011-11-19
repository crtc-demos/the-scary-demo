#ifndef SPOOKY_GHOST_H
#define SPOOKY_GHOST_H 1

#include "timing.h"
#include "shader.h"
#include "object.h"
#include "lighting-texture.h"
#include "scene.h"

typedef struct
{
  lighting_texture_info *lighting_texture;
  object_loc obj_loc;

  float deg;
  float deg2;
  float lightdeg;

  float bla;

  void *lightmap;
  void *reflection;

  GXTexObj texture;
  GXTexObj bumpmap;
  GXTexObj reflection_obj;

  shader_info *bump_mapping_shader;
  shader_info *tunnel_lighting_shader;
  shader_info *water_shader;
} spooky_ghost_data;

extern effect_methods spooky_ghost_methods;
extern spooky_ghost_data spooky_ghost_data_0;

#endif
