/* SPDX-License-Identifier: MIT */

#ifndef LOADER_BUILTIN_PIC_TEST_COMMON_H
#define LOADER_BUILTIN_PIC_TEST_COMMON_H

#include "loader_builtin_memory_test_common.h"

void edge_pic_begin(edge_writer_t *writer,
                    unsigned int width,
                    unsigned int height);
void edge_pic_packet(edge_writer_t *writer,
                     int chained,
                     unsigned int type,
                     unsigned int channels);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
