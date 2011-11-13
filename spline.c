#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "spline.h"
#include "server.h"

static float
knot_vector (spline_info *spline, int index)
{
  int val = index - spline->order + 1;
  if (val < 0)
    return 0;
  else if (val > spline->length - 1)
    return spline->length - 1;
  else
    return val;
}

static float
f (spline_info *spline, int i, int n, float u)
{
  float k_i = knot_vector (spline, i);
  return (u - k_i) / (knot_vector (spline, i + n) - k_i);
}

static float
g (spline_info *spline, int i, int n, float u)
{
  float k_i1 = knot_vector (spline, i + n);
  return (k_i1 - u) / (k_i1 - knot_vector (spline, i));
}

static float
basis (spline_info *spline, int i, int n, float u)
{
  if (n == 0)
    {
      float knot_i = knot_vector (spline, i);
      float knot_i1 = knot_vector (spline, i + 1);
      if (knot_i <= u && u < knot_i1 && knot_i < knot_i1)
        return 1;
      else
        return 0;
    }
  else
    {
      float basis1 = basis (spline, i, n - 1, u);
      float tmp1 = basis1 == 0 ? 0 : f (spline, i, n, u) * basis1;
      float basis2 = basis (spline, i + 1, n - 1, u);
      float tmp2 = basis2 == 0 ? 0 : g (spline, i + 1, n, u) * basis2;
      return tmp1 + tmp2;
    }
}

static void
evaluate_spline_at_param (spline_info *spline, float *dest, float u)
{
  float res[4] = { 0.0, 0.0, 0.0, 0.0 };
  int i, dim;
  float norm_factor = 0;
  
  for (i = 0; i < spline->length; i++)
    {
      float basis_val = basis (spline, i, spline->order, u);
      float weight = spline->weight[i];
      float weighted_basis = weight * basis_val;
      for (dim = 0; dim < 4; dim++)
	res[dim] += weighted_basis * (*spline->knots)[i][dim];
      norm_factor += weighted_basis;
    }
  
  for (dim = 0; dim < 4; dim++)
    dest[dim] = res[dim] / norm_factor;
}

static float
dist (float *thispt, float *prevpt)
{
  return sqrtf (
    (thispt[0] - prevpt[0]) * (thispt[0] - prevpt[0])
     + (thispt[1] - prevpt[1]) * (thispt[1] - prevpt[1])
     + (thispt[2] - prevpt[2]) * (thispt[2] - prevpt[2])
     + (thispt[3] - prevpt[3]) * (thispt[3] - prevpt[3]));
}

/* To follow blender, we should evaluate the number of points given by:

   resolution * (spline_length - 1)
   
   then join them up piecewise-linearly.  */

void
evaluate_spline (spline_info *spline)
{
  int entries = spline->resolution * (spline->length - 1), i;
  evaluated_spline_info *eval;
  float length;

  if (spline->eval == NULL)
    {
      eval = malloc (sizeof (evaluated_spline_info));

      eval->points = malloc (4 * sizeof (float) * entries);
      eval->accum_length = malloc (sizeof (float) * entries);
      eval->entries = entries;

      srv_printf ("allocated memory for spline evaluation\n");

      spline->eval = eval;
    }
  else
    eval = spline->eval;
  
  for (i = 0; i < entries; i++)
    {
      float param = (float) (i * (spline->length - 1)) / (entries - 1);
      float pos[4];
      if (i == 0)
        memcpy (pos, (*spline->knots)[0], sizeof (float) * 4);
      else if (i == entries - 1)
        memcpy (pos, (*spline->knots)[spline->length - 1], sizeof (float) * 4);
      else
        evaluate_spline_at_param (spline, pos, param);
/*      srv_printf ("entry %d, param %f: %f,%f,%f,%f\n", i, param, pos[0],
		  pos[1], pos[2], pos[3]);*/
      memcpy ((*eval->points)[i], pos, sizeof (float) * 4);
    }
  
  eval->accum_length[0] = 0.0;
  
  for (i = 1, length = 0; i < entries; i++)
    {
      float *thispt = (*eval->points)[i];
      float *prevpt = (*eval->points)[i - 1];
      float seg_length = dist (thispt, prevpt);

      length += seg_length;
      eval->accum_length[i] = length;
      /*srv_printf ("accumulated length at %d: %f\n", i,
		    eval->accum_length[i]);*/
    }
}

void
free_spline_data (spline_info *spline)
{
  free (spline->eval->points);
  free (spline->eval->accum_length);
  free (spline->eval);
  spline->eval = NULL;
}

/* Return the index of the line section we're in.  */

static int
find_section (float *arr, int len, float thing)
{
  int lo = 0, hi = len - 1;
  
  while (hi - lo > 1)
    {
      int midpt = (lo + hi) / 2;

      /*srv_printf ("searching for %f: lo=%d, hi=%d\n", thing, lo, hi);*/

      if (thing < arr[midpt])
        hi = midpt;
      else if (thing > arr[midpt])
        lo = midpt;
    }
  
  return lo;
}

static float
lerp (float a, float b, float i)
{
  return (1 - i) * a + i * b;
}

/* Evaluate point at distance along piecewise-linear representation of the
   spline.  Parameter U is normalized to 0...1.  */

void
get_evaluated_spline_pos (spline_info *spline, float *dst, float u)
{
  evaluated_spline_info *eval = spline->eval;
  float highest_length = eval->accum_length[eval->entries - 1];
  int idx, dim;
  float line_start, sec_length;
  float *lo_pt, *hi_pt, sec_param;
  
  if (u < 0)
    u = 0;
  else if (u > 1)
    u = 1;
  
  u *= highest_length;
  
  idx = find_section (eval->accum_length, eval->entries, u);
  
  line_start = eval->accum_length[idx];
  sec_length = eval->accum_length[idx + 1] - line_start;
  
  if (u < line_start || u >= (line_start + sec_length))
    srv_printf ("bad section: line_start=%f, u=%f, sec_length=%f, idx=%d\n",
		line_start, u, sec_length, idx);
  
  lo_pt = (*eval->points)[idx];
  hi_pt = (*eval->points)[idx + 1];
  
  sec_param = (u - line_start) / sec_length;
  
  for (dim = 0; dim < 4; dim++)
    dst[dim] = lerp (lo_pt[dim], hi_pt[dim], sec_param);
}
