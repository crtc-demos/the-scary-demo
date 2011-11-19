#include <stdlib.h>
#include <string.h>

#include "tracking-obj.h"

void
tracking_obj_pos (guVector *dst, tracking_obj_base *tracking_obj, float param)
{
  switch (tracking_obj->type)
    {
    case TRACKING_OBJ_STATIC:
      {
        static_tracking_obj *stat = (static_tracking_obj *) tracking_obj;
	dst->x = stat->pos[0];
	dst->y = stat->pos[1];
	dst->z = stat->pos[2];
      }
      break;
    
    case TRACKING_OBJ_PATH:
      {
        float pos[4];
	spline_tracking_obj *sto = (spline_tracking_obj *) tracking_obj;
	get_evaluated_spline_pos (&sto->spline, pos, param);
	dst->x = pos[0];
	dst->y = pos[1];
	dst->z = pos[2];
      }
      break;
    
    default:
      abort ();
    }
}
