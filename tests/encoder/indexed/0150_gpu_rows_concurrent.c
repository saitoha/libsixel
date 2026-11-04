/* SPDX-License-Identifier: MIT */
/* Independent sessions retain their own output while sharing the backend.
 * Policy: docs/threading/encoder.md
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "tests/_support/processing/gpu-palette/gpu_rows_common.h"
#include "src/threading.h"

int
test_gpu_rows_concurrent(int argc, char **argv)
{
#if !SIXEL_ENABLE_THREADS
    (void)argc;
    (void)argv;
    return 77;
#else
    gpu_rows_fixture_t f;
    int outcome;
    gpu_rows_fixture_t other;
    sixel_thread_t thread;
    int other_opened;
    int running;
    int i;

    (void)argc;
    (void)argv;
    other_opened = running = 0;
    outcome = gpu_rows_open(&f, 1);
    if (outcome != 0) {
        gpu_rows_close(&f);
        return outcome;
    }
    outcome = 1;
    other_opened = 1;
    ROWS_CHECK(gpu_rows_open(&other, 1) == 0);
    memset(other.pixels, 0, sizeof(other.pixels));
    ROWS_CHECK(sixel_thread_create(&thread, gpu_rows_worker, &other) == 0);
    running = 1;
    ROWS_CHECK(gpu_rows_run(&f) == SIXEL_OK);
    sixel_thread_join(&thread);
    running = 0;
    ROWS_CHECK(other.status == SIXEL_OK);
    for (i = 0; i < GPU_ROWS_PIXELS; ++i) {
        ROWS_CHECK(f.output[i] == (unsigned char)(i % 3 != 0) &&
            other.output[i] == 0);
    }
    outcome = 0;
end:
    if (running) sixel_thread_join(&thread);
    if (other_opened) gpu_rows_close(&other);
    gpu_rows_close(&f);
    return outcome;
#endif
}
