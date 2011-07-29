#ifndef OBJECT_H
#define OBJECT_H 1

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
    .num_strips = ARRAY_SIZE (NAME##_strips)	\
  }

#define OBJECT_POS	1
#define OBJECT_NORM	2
#define OBJECT_TEXCOORD	4
#define OBJECT_NBT3	8

extern void object_set_arrays (object_info *, unsigned int, int, int);
extern void object_render (object_info *, unsigned int, int);

#endif
