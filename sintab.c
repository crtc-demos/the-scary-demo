#include <math.h>

float sintab[256];

void
fastsin_init (void)
{
  int i;
  
  for (i = 0; i < 256; i++)
    sintab[i] = sin (i * M_PI * 2.0 / 256.0);
}
