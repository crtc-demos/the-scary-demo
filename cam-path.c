#include "cam-path.h"
#include "scene.h"
#include "tracking-obj.h"

void
cam_path_follow (scene_info *scene, Mtx path_mtx, cam_path *campath,
		 float param)
{
  guVector tmp;
  
  tracking_obj_pos (&tmp, campath->track_to, param);
  guVecMultiply (path_mtx, &tmp, &tmp);
  scene_set_lookat (scene, tmp);
  
  tracking_obj_pos (&tmp, campath->follow_path, param);
  guVecMultiply (path_mtx, &tmp, &tmp);
  scene_set_pos (scene, tmp);
  
  scene_update_camera (scene);
}
