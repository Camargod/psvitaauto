#ifndef PSVITAAUTO_AVCDEC_H
#define PSVITAAUTO_AVCDEC_H

#include <stdint.h>

#include <psp2/videodec.h>

typedef struct avcdec avcdec;

/* Create an H.264 hardware decoder for width x height. Returns NULL on failure,
 * and writes the failing return code into *err_out (0 on success). */
avcdec *avcdec_create(uint32_t width, uint32_t height, uint32_t num_ref_frames, int *err_out);
void avcdec_destroy(avcdec *d);

/* Decode one access unit (Annex-B H.264 NALs). Returns 0 on success. */
int avcdec_decode(avcdec *d, const uint8_t *data, uint32_t size);

/* Configure the RGBA8888 output target. out must hold out_width*out_height*4
 * bytes (e.g. a vita2d texture data pointer). The decoder scales the frame to
 * this size in hardware. */
void avcdec_set_output(avcdec *d, void *out, uint32_t out_width, uint32_t out_height);

/* The most recently decoded picture, or NULL if none. The picture's frame
 * carries pixelType, framePitch, frameWidth/Height and pPicture[] buffers. */
const SceAvcdecPicture *avcdec_last_picture(avcdec *d);

/* Diagnostic info from the last avcdec_create() attempt, for on-screen debug.
 * step: 1=QueryDecoderMemSize 2=InitLibrary 3=AllocMemBlock 4=CreateDecoder. */
int avcdec_last_step(void);
int avcdec_last_rc(void);
uint32_t avcdec_last_framesize(void);

#endif
