#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void i2p_macos_nsview_apply_vibrancy(void *nsview, int night, int dialog);
void i2p_macos_nsview_round_corners(void *nsview, int left_side);
void i2p_macos_nsview_clip_rounded(void *nsview, double radius, unsigned int border_rgb, int which);

#ifdef __cplusplus
}
#endif
