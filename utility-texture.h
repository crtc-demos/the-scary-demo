#ifndef UTILITY_TEXTURE_H
#define UTILITY_TEXTURE_H 1

#include <ogcsys.h>
#include <stdlib.h>

typedef struct {
  char *texbuf;
  GXTexObj texobj;
  u32 format;
} utility_texture_info;

typedef enum {
  UTIL_TEX_8BIT_RAMP,
  UTIL_TEX_16BIT_RAMP,
  UTIL_TEX_16BIT_RAMP_NOREPEAT,
  UTIL_TEX_REFRACT,
  UTIL_TEX_DARKENING,
  UTIL_TEX_MAX
} utility_texture_handle;

extern unsigned int tex_index (unsigned int, unsigned int, unsigned int,
			       unsigned int);

/* Get a cached GXTexObj for the given utility texture, creating it if
   necessary.  */
extern GXTexObj *get_utility_texture (utility_texture_handle);

/* Free a utility texture -- it will be automatically regenerated if requested
   again.  */
extern void free_utility_texture (utility_texture_handle);

#endif
