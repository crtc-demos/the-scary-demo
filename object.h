#ifndef OBJECT_H
#define OBJECT_H 1

#include "shadow.h"
#include "scene.h"

typedef struct object_loc_tag {
  /* Which position/normal matrix to use (GX_PNMTX0 etc.).  */
  u32 pnmtx;
  
  char calculate_normal_tex_mtx;
  char calculate_sph_envmap_tex_mtx;
  char calculate_binorm_tex_mtx;
  char calculate_vertex_depth_mtx;
  char calculate_screenspace_tex_mtx;
  char calculate_shadowing_tex_mtx;
  char calculate_parallax_tex_mtx;
  u32 normal_tex_mtx;
  u32 sph_envmap_tex_mtx;
  u32 binorm_tex_mtx;
  u32 vertex_depth_mtx;
  u32 screenspace_tex_mtx;
  struct {
    u32 binorm_tex_mtx;
    u32 tangent_tex_mtx;
    u32 texture_edge;
  } parallax;
  struct {
    u32 buf_tex_mtx[3];
    u32 ramp_tex_mtx;
    shadow_info *info;
    bool multisample;
    float separation;
  } shadow;
  /* Now then... this whole thing was designed to only have one of these locs
     per object.  If we want to layer multiple effects (e.g. multiple shadows),
     use this, and all the chained locs will be calculated too...  chained locs
     won't set projection matrixes  */
  struct object_loc_tag *chained_loc;
} object_loc;

typedef struct {
  f32 *positions;
  f32 *normals;
  f32 *texcoords;
  u16 **obj_strips;
  unsigned int *strip_lengths;
  unsigned int num_strips;
} object_info;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(X) (sizeof (X) / sizeof (X[0]))
#endif

#define INIT_OBJECT(VAR, NAME)			\
  object_info VAR = {				\
    .positions = NAME##_pos,			\
    .normals = NAME##_norm,			\
    .texcoords = NAME##_texidx,			\
    .obj_strips = NAME##_strips,		\
    .strip_lengths = NAME##_lengths,		\
    .num_strips = ARRAY_SIZE (NAME##_strips),	\
  }

#define OBJECT_POS	1
#define OBJECT_NORM	2
#define OBJECT_TEXCOORD	4
#define OBJECT_NBT3	8

extern void object_loc_initialise (object_loc *obj, u32 pnmtx);
extern void object_set_tex_norm_binorm_matrices (object_loc *,
  u32 normal_tex_mtx, u32 binorm_tex_mtx);
extern void object_unset_tex_norm_binorm_matrices (object_loc *);
extern void object_set_tex_norm_matrix (object_loc *loc, u32 normal_tex_mtx);
extern void object_unset_tex_norm_matrix (object_loc *loc);
extern void object_set_sph_envmap_matrix (object_loc *loc, u32 normal_tex_mtx);
extern void object_unset_sph_envmap_matrix (object_loc *loc);
extern void object_set_vertex_depth_matrix (object_loc *, u32);
extern void object_unset_vertex_depth_matrix (object_loc *);
extern void object_set_screenspace_tex_matrix (object_loc *, u32);
extern void object_unset_screenspace_tex_matrix (object_loc *);
extern void object_set_parallax_tex_matrices (object_loc *, u32, u32, u32);
extern void object_unset_parallax_tex_matrices (object_loc *);
extern void object_set_shadow_tex_matrix (object_loc *loc,
					  u32 shadow_buf_tex_mtx,
					  u32 shadow_ramp_tex_mtx,
					  shadow_info *);
extern void object_set_multisample_shadow_tex_matrix (object_loc *,
  u32 shadow_buf_tex_mtx_0, u32 shadow_buf_tex_mtx_1, u32 shadow_buf_tex_mtx_2,
  u32 shadow_ramp_tex_mtx, float separation, shadow_info *shinf);
extern void object_unset_shadow_tex_matrix (object_loc *loc);
extern void object_set_pos_norm_matrix (object_loc *obj, u32 pnmtx);
extern void object_set_chained_loc (object_loc *loc, object_loc *next_loc);
extern void object_set_matrices (scene_info *scene, object_loc *obj,
				 Mtx cam_mtx, Mtx obj_mtx, Mtx sep_scale,
				 Mtx projection, u32 proj_type);

extern void object_set_arrays (object_info *, unsigned int, int, int);
extern void object_render (object_info *, unsigned int, int);

#endif
