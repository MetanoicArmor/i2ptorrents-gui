#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void i2p_macos_nsview_apply_vibrancy(void *nsview, int night, int dialog);
void i2p_macos_nsview_apply_opaque_dialog(void *nsview, int night);
void i2p_macos_nsview_apply_menu_vibrancy(void *nsview, int night, double radius);
void i2p_macos_nsview_round_corners(void *nsview, int left_side);
void i2p_macos_nsview_clip_rounded(void *nsview, double radius, unsigned int border_rgb, int which,
                                   unsigned int fill_rgb);
void i2p_macos_nsview_clip_control(void *nsview, double radius, unsigned int fill_rgb);

#ifdef __cplusplus
}
#endif
