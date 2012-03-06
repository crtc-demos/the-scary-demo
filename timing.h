#ifndef TIMING_H
#define TIMING_H 1

#include <stdint.h>
#include <ogcsys.h>

#include "backbuffer.h"

typedef enum {
  MAIN_BUFFER,
  BACKBUFFER_0,
  BACKBUFFER_1,
  BACKBUFFER_2,
  BACKBUFFER_3
} display_target;

/* These are the elapsed time since the effect in question started, and some
   global parameters which can be used to control stuff.  */

typedef struct {
  uint32_t time_offset;
  float param1;
  float param2;
  float param3;
} sync_info;

typedef struct {
  void (*preinit_assets) (void);
  void (*init_effect) (void *params, backbuffer_info *bbufs);
  display_target (*prepare_frame) (sync_info *sync, void *params,
				   int iparam);
  void (*display_effect) (sync_info *sync, void *params, int iparam);
  void (*composite_effect) (sync_info *sync, void *params, int iparam,
			    backbuffer_info *);
  void (*uninit_effect) (void *params, backbuffer_info *bbufs);
  void (*finalize) (void *params);
} effect_methods;

typedef struct {
  uint64_t start_time;
  uint64_t end_time;
  effect_methods *methods;
  void *params;
  int iparam;
  int finalize;
} do_thing_at;

#ifndef NULL
#define NULL ((void*) 0)
#endif

#endif
