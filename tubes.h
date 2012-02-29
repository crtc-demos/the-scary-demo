#ifndef TUBES_H
#define TUBES_H 1

#include "timing.h"
#include "world.h"
#include "shader.h"
#include "shadow.h"

typedef struct
{
  world_info *world;
  shader_info *tube_shader;
  
  Mtx modelview;
  
  struct {
    shadow_info *info;
    void *buf;
    GXTexObj buf_texobj;
  } shadow[2];

  GXTexObj snakeskin_texture_obj;
  object_loc tube_locator;
  object_loc tube_locator_2; /* For second shadow.  */
} tube_data;

extern effect_methods tubes_methods;
extern tube_data tube_data_0;

#endif
