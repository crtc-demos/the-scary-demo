#ifndef SHADOW_H
#define SHADOW_H 1

#include "light.h"
#include "utility-texture.h"

typedef struct {
  Mtx44 shadow_projection;
  Mtx shadow_tex_projection;
  Mtx shadow_tex_depth;
  u32 projection_type;
  light_info *light;
  Mtx light_cam;
  
  utility_texture_handle ramp_type;
  
  /* Bounding sphere radius for the object casting the shadow.  The light
     "lookat" position is used as the centre of the object.  */
  float target_radius;
} shadow_info;

extern shadow_info *create_shadow_info (int entries, light_info *light);
extern void destroy_shadow_info (shadow_info *);
extern void shadow_set_bounding_radius (shadow_info *, float);
extern void shadow_setup_ortho (shadow_info *, float, float);
extern void shadow_setup_frustum (shadow_info *, float, float);
extern void shadow_casting_tev_setup (void *);

#endif
