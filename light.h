#ifndef LIGHT_H
#define LIGHT_H 1

typedef struct
{
  guVector pos;
  guVector lookat;
  guVector up;

  guVector tpos;
  guVector tlookat;
} light_info;

#endif
