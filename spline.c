#include "spline.h"

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

void
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
