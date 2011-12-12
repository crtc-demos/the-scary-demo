#ifndef SINTAB_H
#define SINTAB_H 1

extern float sintab[256];

/* Fast sin/cos, reduced accuracy in domain: still use range 0-2*pi though.  */
#define FASTSIN(X) (sintab[(int) ((X) * 256.0 / (2.0 * M_PI)) & 255])
#define FASTCOS(X) FASTSIN(X + M_PI / 2)

/* These ones use 0-255 for the domain.  */
#define SIN256(X) (sintab[(int) (X) & 255])
#define COS256(X) (sintab[(int) ((X) + 64) & 255])

extern void fastsin_init (void);

#endif
