#ifndef GLASS_H
#define GLASS_H 1

#include "object.h"
#include "shader.h"

typedef struct
{
  object_loc obj_loc;
  int thr;
  void *grabbed_texture;

  shader_info *refraction_shader;
  shader_info *refraction_postpass_shader;
  GXTexObj grabbed_tex_obj;
  GXTexObj mighty_zebu_tex_obj;

  shader_info *glass_postpass_shader;
} glass_data;

extern effect_methods glass_methods;
extern glass_data glass_data_0;

#endif
