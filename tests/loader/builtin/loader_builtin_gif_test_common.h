/* SPDX-License-Identifier: MIT */

#ifndef LOADER_BUILTIN_GIF_TEST_COMMON_H
#define LOADER_BUILTIN_GIF_TEST_COMMON_H

#include "loader_builtin_memory_test_common.h"

extern unsigned char const edge_palette_rgb[12];

void edge_gif_begin(edge_writer_t *writer,
                    int version_89,
                    unsigned int width,
                    unsigned int height);
void edge_gif_image(edge_writer_t *writer,
                    unsigned int x,
                    unsigned int y,
                    unsigned int width,
                    unsigned int height,
                    int interlaced,
                    unsigned char const *pixels,
                    size_t pixel_count,
                    int clear_each);
void edge_gif_graphic_control(edge_writer_t *writer,
                              unsigned int disposal);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
