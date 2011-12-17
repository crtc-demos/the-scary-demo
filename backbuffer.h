#ifndef BACKBUFFER_H
#define BACKBUFFER_H 1

#include <gccore.h>
#include <ogcsys.h>

#define MAX_BACKBUFFERS 4

typedef struct {
  GXTexObj texobj;
  void *pixels;
  u16 width;
  u16 height;
  u8 texfmt;
  u8 copyfmt;
  int initialized;
} backbuffer_info;

#endif
