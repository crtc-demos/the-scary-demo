#ifndef WORLD_H
#define WORLD_H 1

/* A basic kind of retained-mode scenegraph.  */

#include "scene.h"
#include "light.h"
#include "object.h"
#include "shader.h"
#include "list.h"

typedef struct
{
  object_info *object;
  object_loc *loc;
  unsigned int object_flags;
  u32 texture_coord;
  u8 vtxfmt;
  
  Mtx *modelview;
  Mtx *sep_scale;
  
  shader_info *shader;
} standard_object;

MAKE_LIST_OF (standard_object)

typedef struct
{
  Mtx44 projection;
  u32 projection_type;
  
  /* It's sometimes useful to remember these...  */
  float near;
  float far;
  
  scene_info scene;
  
  light_info *light;
  u32 num_lights;
  
  standard_object_list standard_objects;
} world_info;

extern world_info *create_world (int num_lights);
extern void free_world (world_info *world);
extern void world_set_perspective (world_info *world, float fov, float aspect,
				   float near, float far);
extern void world_set_pos_lookat_up (world_info *world, guVector pos,
				     guVector lookat, guVector up);
extern void world_set_light_pos_lookat_up (world_info *world, int num,
					   guVector pos, guVector lookat,
					   guVector up);
extern standard_object *world_add_standard_object (world_info *world,
  object_info *object, object_loc *loc, unsigned int object_flags,
  unsigned int vtxfmt, unsigned int texture_coord, Mtx *modelview,
  Mtx *sep_scale, shader_info *shader);
extern void world_display (world_info *world);

#endif
