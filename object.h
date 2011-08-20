#ifndef OBJECT_H
#define OBJECT_H 1

typedef struct {
  /* Which position/normal matrix to use (GX_PNMTX0 etc.).  */
  u32 pnmtx;
  
  char calculate_normal_tex_mtx;
  char calculate_binorm_tex_mtx;
  char calculate_vertex_depth_mtx;
  u32 normal_tex_mtx;
  u32 binorm_tex_mtx;
  u32 vertex_depth_mtx;
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
extern void object_set_vertex_depth_mtx (object_loc *, u32);
extern void object_unset_vertex_depth_mtx (object_loc *);
extern void object_set_pos_norm_matrix (object_loc *obj, u32 pnmtx);

extern void object_set_arrays (object_info *, unsigned int, int, int);
extern void object_render (object_info *, unsigned int, int);

#endif
