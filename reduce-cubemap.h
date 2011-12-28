#ifndef REDUCE_CUBEMAP_H
#define REDUCE_CUBEMAP_H 1

typedef struct
{
  GXTexObj tex[6];
  void *texels[6];
  shader_info *face_shader[6];
  u32 size;
  u32 fmt;
  
  GXTexObj spheretex;
  void *sphtexels;
  u32 sphsize;
  u32 sphfmt;
} cubemap_info;

extern cubemap_info *create_cubemap (u32 size, u32 fmt, u32 ssize, u32 sfmt);
extern void free_cubemap (cubemap_info *cubemap);
extern void cubemap_cam_matrix_for_face (Mtx cam, scene_info *scene, int face);
extern void reduce_cubemap (cubemap_info *cubemap, int subdiv);

#endif
