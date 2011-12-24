#include <string.h>

#include "world.h"

FREE_NODE (standard_object, node)
{
  /* Note: We don't own any of the object's linked data.  Don't free
     anything.  */
}

world_info *
create_world (int num_lights)
{
  world_info *world = malloc (sizeof (world_info));
  
  memset (world, 0, sizeof (world_info));
  
  world->light = calloc (num_lights, sizeof (light_info));
  world->standard_objects = EMPTY (standard_object);
  
  return world;
}

void
free_world (world_info *world)
{
  list_free (standard_object, world->standard_objects);
  free (world->light);
  free (world);
}

void
world_set_perspective (world_info *world, float fov, float aspect, float near,
		       float far)
{
  world->near = near;
  world->far = far;
  
  guPerspective (world->projection, fov, aspect, near, far);
  world->projection_type = GX_PERSPECTIVE;
}

void
world_set_pos_lookat_up (world_info *world, guVector pos, guVector lookat,
			 guVector up)
{
  scene_set_pos (&world->scene, pos);
  scene_set_lookat (&world->scene, lookat);
  scene_set_up (&world->scene, up);
}

void
world_set_light_pos_lookat_up (world_info *world, int num, guVector pos,
			       guVector lookat, guVector up)
{
  world->light[num].pos = pos;
  world->light[num].lookat = lookat;
  world->light[num].up = up;
}

standard_object *
world_add_standard_object (world_info *world, object_info *object,
			   object_loc *loc, unsigned int object_flags,
			   unsigned int vtxfmt, unsigned int texture_coord,
			   Mtx *modelview, Mtx *sep_scale, shader_info *shader)
{
  standard_object obj;
  
  obj.object = object;
  obj.loc = loc;
  obj.object_flags = object_flags;
  obj.vtxfmt = vtxfmt;
  obj.texture_coord = texture_coord;
  obj.modelview = modelview;
  obj.sep_scale = sep_scale;
  obj.shader = shader;
  
  world->standard_objects = list_cons (obj, world->standard_objects);
  
  return &world->standard_objects->data;
}

void
world_display (world_info *world)
{
  int i;
  shader_info *last_shader = NULL;
  int first = 1;

  /* Refresh camera matrix.  */
  scene_update_camera (&world->scene);
  
  /* Transform light position relative to camera.  */
  for (i = 0; i < world->num_lights; i++)
    light_update (world->scene.camera, &world->light[i]);
  
  standard_object *objp;
  
  list_iter_ptr (objp, world->standard_objects)
    {
      Mtx44 *this_projection;

      /* Hmm, shaders which want to see the lights might be a bit awkward, in
         general...
	 Make a small attempt at not reconfiguring shader unnecessarily.  */
      if (objp->shader != last_shader)
        shader_load (objp->shader);

      last_shader = objp->shader;
      
      if (first)
        this_projection = &world->projection;
      else
        this_projection = NULL;
      
      first = 0;
      
      /* Set up matrices for rendering the object.  Avoid reloading the
         projection matrix unnecessarily.  */
      object_set_matrices (&world->scene, objp->loc, world->scene.camera,
			   *objp->modelview,
			   objp->sep_scale ? *objp->sep_scale : NULL,
			   this_projection ? *this_projection : NULL,
			   world->projection_type);

      /* Set up vertex array pointers for rendering object.  */
      object_set_arrays (objp->object, objp->object_flags, objp->vtxfmt,
			 objp->texture_coord);

      /* And render it.  */
      object_render (objp->object, objp->object_flags, objp->vtxfmt);
    }
}
