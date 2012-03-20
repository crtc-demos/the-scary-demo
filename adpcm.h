#ifndef ADPCM_H
#define ADPCM_H 1

extern void adpcm_init (void);
extern u32 adpcm_load_file (const char *filename);
extern u32 adpcm_play (u32 handle, u32 skip_to_millis);
extern void adpcm_stop (void);

#endif
