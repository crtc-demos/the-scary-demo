#ifndef GREETS_H
#define GREETS_H 1

#include "timing.h"
#include "world.h"

typedef struct
{
  world_info *world;
  
  GXTexObj fontobj;
  GXTexObj tileidxobj;
  
  void *tileidx;
  
  shader_info *tile_shader;
  
  object_loc greets_loc;
} greets_data;

extern effect_methods greets_methods;
extern greets_data greets_data_0;

#endif
