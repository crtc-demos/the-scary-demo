#include <stdlib.h>

#include "shadow.h"
#include "utility_texture.h"

/* Make a ramp texture. RAMPBITS can be 8 or 16.  */

shadow_info *
create_shadow_info (int rampbits, light_info *light)
{
  shadow_info *shinf;
  unsigned int i;
  char *ramptex;
  unsigned int ramptexfmt;
  unsigned int ramptexsize;
  
  shinf = calloc (1, sizeof (shadow_info));
  
  switch (entries)
    {
    case 8:
      shinf->ramp_type = UTIL_TEX_8BIT_RAMP;
      break;

    case 16:
      shinf->ramp_type = UTIL_TEX_16BIT_RAMP;
      break;

    default:
      abort ();
    }

  shinf->light = light;

  return shinf;
}

void
destroy_shadow_info (shadow_info *shinf)
{
  free_utility_texture (shinf->ramp_type);
  free (shinf);
}
