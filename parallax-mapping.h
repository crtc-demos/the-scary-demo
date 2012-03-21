#ifndef PARALLAX_MAPPING_H
#define PARALLAX_MAPPING_H 1

#include "timing.h"
#include "lighting-texture.h"
#include "shader.h"
#include "object.h"
#include "skybox.h"

typedef struct
{
  object_loc obj_loc;

  lighting_texture_info *lighting_texture;

  void *texcoord_map;
  void *texcoord_map2;

  float phase;
  float phase2;

  Mtx modelview;
  Mtx scale;

  GXTexObj stone_tex_obj;
  GXTexObj stone_depth_obj;
  GXTexObj height_bump_obj;
  GXTexObj texcoord_map_obj;
  GXTexObj texcoord_map2_obj;

  shader_info *tunnel_lighting_shader;
  shader_info *parallax_lit_phase1_shader;
  shader_info *parallax_lit_phase2_shader;
  shader_info *parallax_lit_phase3_shader;
  
  GXTexObj column_texture_obj;
  shader_info *column_shader;
  object_loc column_loc;
  
  GXTexObj fog_texture_obj;
  shader_info *fog_shader;
  object_loc fog_loc;
  
  void *column_dl;
  u32 column_dl_size;
} parallax_mapping_data;

extern effect_methods parallax_mapping_methods;
extern parallax_mapping_data parallax_mapping_data_0;

#endif
