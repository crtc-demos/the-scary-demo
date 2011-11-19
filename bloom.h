#ifndef BLOOM_H
#define BLOOM_H 1

#include "timing.h"
#include "shadow.h"
#include "shader.h"

typedef struct
{
  float phase;
  float phase2;

  void *stage1_texture;
  void *stage2_texture;
  void *shadow_buf;

  shadow_info *shadow_inf;

  GXTexObj shadowbuf_tex_obj;
  
  shader_info *cube_lighting_shader;

  shader_info *gaussian_blur_shader;
  GXTexObj stage1_tex_obj;

  shader_info *gaussian_blur2_shader;
  GXTexObj stage2_tex_obj;
  
  shader_info *composite_shader;
} bloom_data;

extern effect_methods bloom_methods;
extern bloom_data bloom_data_0;

#endif
