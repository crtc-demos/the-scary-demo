#ifndef SPLINE_H
#define SPLINE_H 1

typedef struct
{
  unsigned int length;
  unsigned int order;
  unsigned int resolution;
  float (*knots)[][4];
  float *tilt;
  float *weight;
} spline_info;

extern void evaluate_spline_at_param (spline_info *, float *dst, float u);

#endif
