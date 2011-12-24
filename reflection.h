#ifndef REFLECTION_H
#define REFLECTION_H 1

#include "skybox.h"
#include "reduce-cubemap.h"

typedef struct
{
  skybox_info *skybox;
  cubemap_info *cubemap;
  shader_info *plain_texture_shader;
} reflection_data;

extern effect_methods reflection_methods;
extern reflection_data reflection_data_0;

#endif
