#ifndef CAM_PATH_H
#define CAM_PATH_H 1

#include "tracking-obj.h"
#include "scene.h"

typedef struct
{
  tracking_obj_base *follow_path;
  tracking_obj_base *track_to;
} cam_path;

extern void cam_path_follow (scene_info *, Mtx, cam_path *, float);

#endif
