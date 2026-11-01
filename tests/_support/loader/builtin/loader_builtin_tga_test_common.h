/* SPDX-License-Identifier: MIT */

#ifndef LOADER_BUILTIN_TGA_TEST_COMMON_H
#define LOADER_BUILTIN_TGA_TEST_COMMON_H

#include "loader_builtin_memory_test_common.h"

void edge_tga_begin(edge_writer_t *writer,
                    unsigned int color_map_type,
                    unsigned int image_type,
                    unsigned int palette_length,
                    unsigned int palette_depth,
                    unsigned int width,
                    unsigned int height,
                    unsigned int pixel_depth,
                    unsigned int descriptor);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
