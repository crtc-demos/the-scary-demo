#ifndef LIGHT_H
#define LIGHT_H 1

#include <ogcsys.h>
#include <gccore.h>

typedef struct
{
  guVector pos;
  guVector lookat;
  guVector up;

  guVector tpos;
  guVector tlookat;
} light_info;

extern void light_update (Mtx, light_info *);

#endif
