/* SPDX-License-Identifier: MIT */
#ifndef TESTS_PHASED_FIXTURE_H
#define TESTS_PHASED_FIXTURE_H
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/encode-result.h"
#include "src/dither-policy-phased.h"
#include "src/lookup-policy-ordered-gpu.h"
#include "src/lookup-policy-vptree-gpu.h"
#include "src/gpu-program.h"
#include "src/palette.h"
#include "src/frame.h"
#include "src/delta-residual.h"

#define PHASED_WIDTH 19
#define PHASED_HEIGHT 17
#define PHASED_PIXELS (PHASED_WIDTH * PHASED_HEIGHT)
#define PHASED_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "phased assertion line %d: %s (%s)\n", \
            __LINE__, #condition, sixel_helper_get_additional_message()); \
        goto end; \
    } \
} while (0)

typedef struct phased_fixture {
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_preparer_interface_t *lookup;
    sixel_gpu_dither_preparer_interface_t *dither;
    sixel_palette_apply_interface_t *apply;
    sixel_palette_t *palette;
    sixel_wire_palette_t *wire;
    sixel_retained_plane_interface_t *plane;
    sixel_retained_read_t *read;
    sixel_frame_t *source;
    sixel_indexed_frame_t *image;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_palette_apply_prepare_request_t prepare;
    sixel_palette_apply_run_request_t run;
    unsigned char pixels[PHASED_PIXELS * 3];
} phased_fixture_t;

int phased_batch_fault_check(int);
int phased_batch_equivalence(unsigned int, int, int, int);
int phased_fractional_check(int kind, int ties);
int phased_fractional_reuse_check(int kind);
int phased_fractional_config_check(
    sixel_lookup_policy_configuration_t const *, int);
unsigned int phased_fractional_reference(
    sixel_lookup_policy_configuration_t const *, unsigned char const *,
    unsigned int, double const *);

SIXELSTATUS phased_fixture_baseline(phased_fixture_t *f, int offset);
int phased_fixture_open(phased_fixture_t *f, int require_gpu);
SIXELSTATUS phased_fixture_deferred(phased_fixture_t *f);
void phased_fixture_close(phased_fixture_t *f);
SIXELSTATUS phased_fixture_prepare(phased_fixture_t *f);
SIXELSTATUS phased_fixture_run(phased_fixture_t *f);
#endif
