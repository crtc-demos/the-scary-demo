#ifndef SKYBOX_H
#define SKYBOX_H 1

#define SKYBOX_LEFT 0
#define SKYBOX_FRONT 1
#define SKYBOX_RIGHT 2
#define SKYBOX_BACK 3
#define SKYBOX_TOP 4
#define SKYBOX_BOTTOM 5

typedef struct
{
  int face;
  void *private_data;
} skybox_shader_data;

typedef struct
{
  GXTexObj tex[6];
  shader_info *face_shader[6];
  skybox_shader_data shader_data[6];
  float radius;
  
  object_loc skybox_loc;
} skybox_info;

extern skybox_info *create_skybox (float radius,
				   tev_setup_fn nondefault_shader,
				   void *privdata);
extern void free_skybox (skybox_info *);
extern void skybox_set_matrices (scene_info *, Mtx, skybox_info *, Mtx, u32);
extern void skybox_render (skybox_info *);

#endif
