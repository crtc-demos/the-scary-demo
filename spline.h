#ifndef SPLINE_H
#define SPLINE_H 1

typedef struct
{
  float (*points)[][4];
  float *accum_length;
  int entries;
} evaluated_spline_info;

typedef struct
{
  unsigned int length;
  unsigned int order;
  unsigned int resolution;
  float (*knots)[][4];
  float *tilt;
  float *weight;
  evaluated_spline_info *eval;
} spline_info;

extern void evaluate_spline (spline_info *);
extern void free_spline_data (spline_info *);
extern void get_evaluated_spline_pos (spline_info *spline, float *dst, float u);

#endif
