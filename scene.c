#include <assert.h>
#include <string.h>
#include <math.h>

#include "scene.h"

void
scene_set_pos (scene_info *scene, guVector pos)
{
  scene->pos = pos;
  scene->camera_dirty = 1;
}

void
scene_set_lookat (scene_info *scene, guVector lookat)
{
  scene->lookat = lookat;
  scene->camera_dirty = 1;
}

void
scene_set_up (scene_info *scene, guVector up)
{
  scene->up = up;
  scene->camera_dirty = 1;
}

void
scene_update_camera (scene_info *scene)
{
  if (scene->camera_dirty)
    {
      guLookAt (scene->camera, &scene->pos, &scene->up, &scene->lookat);
      scene->camera_dirty = 0;
    }
}
