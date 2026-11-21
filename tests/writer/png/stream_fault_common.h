/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_TEST_PNG_STREAM_FAULT_COMMON_H
#define SIXEL_TEST_PNG_STREAM_FAULT_COMMON_H

enum png_stream_fault {
    PNG_STREAM_SHORT_WRITE = 1,
    PNG_STREAM_FLUSH_FAILURE = 2,
    PNG_STREAM_CLOSE_FAILURE = 4,
    PNG_STREAM_STDOUT = 8
};

int png_test_stream_fault(int faults);

#endif
