/* SPDX-License-Identifier: MIT */
#ifndef TEST_GPU_ROWS_COMMON_H
#define TEST_GPU_ROWS_COMMON_H
#include <stdio.h>
#include <string.h>
#include "component_common.h"
#include "src/gpu-palette.h"
#define GPU_ROWS_WIDTH 17
#define GPU_ROWS_HEIGHT 43
#define GPU_ROWS_PIXELS (GPU_ROWS_WIDTH * GPU_ROWS_HEIGHT)
#define ROWS_CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "GPU rows check failed: %s:%d\n", __FILE__, __LINE__); \
    goto end; } } while (0)
typedef struct gpu_rows_fixture {
    gpu_component_fixture_t component;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_program_t *program;
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    unsigned char pixels[GPU_ROWS_PIXELS * 3];
    unsigned char history[GPU_ROWS_PIXELS * 3];
    unsigned char valid[GPU_ROWS_PIXELS];
    unsigned char transparent[GPU_ROWS_PIXELS];
    unsigned char mask[GPU_ROWS_PIXELS];
    unsigned char output[GPU_ROWS_PIXELS];
    unsigned char saved[GPU_ROWS_PIXELS];
    int ends[GPU_ROWS_HEIGHT];
    int calls;
    int prefix;
    int immutable;
    int cancel_after;
    SIXELSTATUS callback_status;
    SIXELSTATUS status;
} gpu_rows_fixture_t;
int gpu_rows_open(gpu_rows_fixture_t *f, int kind);
void gpu_rows_close(gpu_rows_fixture_t *f);
SIXELSTATUS gpu_rows_run(gpu_rows_fixture_t *f);
SIXELSTATUS gpu_rows_notify(void *context, int rows);
int gpu_rows_worker(void *context);
#endif
