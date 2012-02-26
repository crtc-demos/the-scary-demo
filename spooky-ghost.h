#ifndef SPOOKY_GHOST_H
#define SPOOKY_GHOST_H 1

#include "timing.h"
#include "shader.h"
#include "object.h"
#include "lighting-texture.h"
#include "scene.h"

typedef enum
{
  GHOST_LEFT, GHOST_RIGHT, GHOST_TOWARDS
} ghost_dir;

typedef struct
{
  scene_info scene;

  lighting_texture_info *lighting_texture;
  object_loc tunnel_section_loc;

  object_loc ghost_loc;

  float deg;
  float deg2;
  float lightdeg;

  float bla;

  void *lightmap;
  void *reflection;

  GXTexObj texture;
  GXTexObj bumpmap;
  GXTexObj reflection_obj;

  struct
    {
      bool visible;
      float acrossness;
      float alongness;
      GXColor colour;
      ghost_dir ghost_dir;
      int sometimes;
      int number;
    } ghost;

  shader_info *bump_mapping_shader;
  shader_info *tunnel_lighting_shader;
  shader_info *ghost_shader;
  shader_info *water_shader;
} spooky_ghost_data;

extern effect_methods spooky_ghost_methods;
extern spooky_ghost_data spooky_ghost_data_0;

#endif
