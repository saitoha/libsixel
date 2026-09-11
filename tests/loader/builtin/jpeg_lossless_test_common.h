/* Shared in-memory JPEG lossless stream construction for direct tests. */

#ifndef JPEG_LOSSLESS_TEST_COMMON_H
#define JPEG_LOSSLESS_TEST_COMMON_H

#include <stddef.h>

size_t jpeg_lossless_build_gray_predictor(unsigned char *buffer,
                                          size_t capacity,
                                          int predictor,
                                          int point_transform);
size_t jpeg_lossless_build_rgb_restart(unsigned char *buffer,
                                       size_t capacity);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
