#ifndef SCENE_H
#define SCENE_H 1

#include <ogcsys.h>
#include <gccore.h>

#include "shadow.h"

/* Basic camera parameters.  */

typedef struct {
  Mtx camera;
  
  guVector pos;
  guVector up;
  guVector lookat;

  /* This is quite a leaky abstraction... this needs to be poked by the caller
     when it's needed.  */
  Mtx depth_ramp_lookup;

  char camera_dirty;
} scene_info;

#define NRM_SCALE 0.8

extern void scene_set_pos (scene_info *scene, guVector pos);
extern void scene_set_lookat (scene_info *scene, guVector lookat);
extern void scene_set_up (scene_info *scene, guVector up);
extern void scene_update_camera (scene_info *scene);
		       
#endif
