#ifndef TENTACLES_H
#define TENTACLES_H 1

#include "timing.h"
#include "world.h"

typedef struct
{
  world_info *world;
  
  shader_info *tentacle_shader;
  object_loc tentacle_loc;
  Mtx tentacle_modelview;
  Mtx tentacle_scale;
  
  float *tentacle_wavey_pos;
  
  float rot;
  
  float wave;
} tentacle_data;

extern effect_methods tentacle_methods;
extern tentacle_data tentacle_data_0;

#endif
