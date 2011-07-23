#ifndef OBJECT_H
#define OBJECT_H 1

typedef struct {
  f32 *positions;
  f32 *normals;
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
    .obj_strips = NAME##_strips,		\
    .strip_lengths = NAME##_lengths,		\
    .num_strips = ARRAY_SIZE (NAME##_strips)	\
  }

extern void object_set_arrays (object_info *, int);
extern void object_render (object_info *, int);

#endif
