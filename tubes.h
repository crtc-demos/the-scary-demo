#ifndef TUBES_H
#define TUBES_H 1

#include "timing.h"
#include "world.h"
#include "shader.h"

typedef struct
{
  world_info *world;
  shader_info *tube_shader;

  GXTexObj snakeskin_texture_obj;
  object_loc tube_locator;
} tube_data;

extern effect_methods tubes_methods;
extern tube_data tube_data_0;

#endif
