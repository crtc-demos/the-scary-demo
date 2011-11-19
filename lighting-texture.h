#ifndef LIGHTING_TEXTURE_H
#define LIGHTING_TEXTURE_H 1

#include "scene.h"

typedef struct {
  GXTexObj texobj;
  void *texbuf;
} lighting_texture_info;

extern lighting_texture_info *create_lighting_texture (void);
extern void free_lighting_texture (lighting_texture_info *lt);

/* Before calling this function you should set up your TEV parameters as you
   require.
   Beware that it:
    * Internally switches rendering context to a texture
    * Messes up the EFB
    * Messes up the vertex format
   so you'd better not call it when doing that would be bad.  */

extern void update_lighting_texture (scene_info *scene,
				     lighting_texture_info *lt);

#endif
