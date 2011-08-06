#ifndef RENDERTARGET_H
#define RENDERTARGET_H 1

#include <ogcsys.h>
#include <gccore.h>

extern void rendertarget_screen (GXRModeObj *rmode);
extern void rendertarget_texture (u32 width, u32 height, u32 texfmt);

#endif
