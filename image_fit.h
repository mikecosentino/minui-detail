#ifndef IMAGE_FIT_H
#define IMAGE_FIT_H

#include <stdint.h>

// Image_Fit gives the largest size with the source's aspect ratio that fits
// inside the box, never smaller than 1x1.
void Image_Fit(int src_w, int src_h, int box_w, int box_h, int *out_w, int *out_h);

// Image_Resample scales 32-bit pixels (any channel order: each byte is
// averaged on its own) from src to dst with a box filter, so every source
// pixel counts towards the result by how much of it each destination pixel
// covers. SDL's own scaled blit picks the nearest pixel instead, which turns
// a 528px cover drawn at 300px into a sparkle of dropped rows and columns.
//
// Pitches are in pixels. Returns 0 if it could not allocate its scratch
// buffers, 1 otherwise.
int Image_Resample(const uint32_t *src, int src_w, int src_h, int src_pitch,
                   uint32_t *dst, int dst_w, int dst_h, int dst_pitch);

#endif
