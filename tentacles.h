#ifndef TENTACLES_H
#define TENTACLES_H 1

#include "timing.h"
#include "world.h"

typedef struct
{
  world_info *world;
  int light_brightness;
  
  shader_info *tentacle_shader;
  object_loc tentacle_loc;
  Mtx tentacle_modelview;
  Mtx tentacle_scale;
  
  Mtx box_scale;
  
  shader_info *channelsplit_shader;
  void *back_buffer;
  GXTexObj backbuffer_texobj;
  
  float *tentacle_wavey_pos;
  
  float rot;
  float rot2;
  float rot3;
  
  float wave;
  
  GXTexRegionCallback prev_callback;
  GXTexRegion texregion;
} tentacle_data;

extern effect_methods tentacle_methods;
extern tentacle_data tentacle_data_0;

#endif
