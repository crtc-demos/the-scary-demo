#ifndef SHADOW_H
#define SHADOW_H 1

#include "light.h"
#include "utility_texture.h"

typedef struct {
  Mtx44 shadow_projection;
  u32 projection_type;
  light_info *light;
  
  utility_texture_handle ramp_type;
} shadow_info;

extern shadow_info *create_shadow_info (int entries, light_info *light);
extern void destroy_shadow_info (shadow_info *);

#endif
