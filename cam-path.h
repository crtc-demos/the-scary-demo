#ifndef CAM_PATH_H
#define CAM_PATH_H 1

#include "tracking-obj.h"

typedef struct
{
  tracking_obj_base *follow_path;
  tracking_obj_base *track_to;
} cam_path;

#endif
