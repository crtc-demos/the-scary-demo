#ifndef SCENE_H
#define SCENE_H 1

#include <ogcsys.h>
#include <gccore.h>

#include "object.h"

/* Basic camera parameters.  */

typedef struct {
  Mtx camera;
  
  guVector pos;
  guVector up;
  guVector lookat;

  Mtx depth_ramp_lookup;

  char camera_dirty;
} scene_info;

#define NRM_SCALE 0.8

extern void scene_set_pos (scene_info *scene, guVector pos);
extern void scene_set_lookat (scene_info *scene, guVector lookat);
extern void scene_set_up (scene_info *scene, guVector up);
extern void scene_update_camera (scene_info *scene);
extern void scene_update_matrices (scene_info *scene, object_loc *obj,
				   Mtx cam_mtx, Mtx obj_mtx);
		       
#endif
