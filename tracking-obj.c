#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "server.h"

#include "tracking-obj.h"

static float
lerp (float a, float b, float p)
{
  return (1.0 - p) * a + p * b;
}

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
	
	/* We might have an animation curve.  Alter param according to it if
	   so.  */
	if (sto->anim_data)
	  {
	    float frame = param * sto->anim_frames;
	    int iframe = (int) frame;
	    float lo, hi, interp;
	    
	    /* Do we need to do anything with anim_start and anim_end?  */
	    
	    if (iframe >= sto->anim_frames - 1)
	      param = 1.0;
	    else
	      {
	        lo = sto->anim_data[iframe];
		hi = sto->anim_data[iframe + 1];
		interp = lerp (lo, hi, frame - floorf (frame));
		param = interp / (float) sto->anim_frames;
	      }
	  }
	
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
