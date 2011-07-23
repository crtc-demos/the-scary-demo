#include "light.h"

void
light_update (Mtx viewmat, light_info *mylight)
{
  guVecMultiply (viewmat, &mylight->pos, &mylight->tpos);
  guVecMultiply (viewmat, &mylight->lookat, &mylight->tlookat);
}
