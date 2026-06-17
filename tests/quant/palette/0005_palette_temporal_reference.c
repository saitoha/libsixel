/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for temporal palette reference weighting.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/factory.h"

static int
generate_single_average(unsigned char const *data,
                        unsigned int data_size,
                        unsigned char const *temporal_data,
                        unsigned char out[3])
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_palette_t *palette;
    sixel_palette_generate_request_t request;
    sixel_palette_entries_view_t entries_view;
    void *service;
    void *object;
    int ok;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    palette = NULL;
    service = NULL;
    object = NULL;
    ok = 0;
    memset(&request, 0, sizeof(request));
    memset(&entries_view, 0, sizeof(entries_view));
    out[0] = 0U;
    out[1] = 0U;
    out[2] = 0U;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
            factory->vtbl->create == NULL ||
            factory->vtbl->unref == NULL) {
        goto end;
    }

    status = factory->vtbl->create(factory, "quant/palette",
                                   allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    palette = (sixel_palette_t *)object;
    object = NULL;
    if (palette == NULL || palette->vtbl == NULL ||
            palette->vtbl->generate == NULL ||
            palette->vtbl->get_entries == NULL ||
            palette->vtbl->unref == NULL) {
        goto end;
    }

    request.data = data;
    request.length = data_size;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.requested_colors = 1U;
    request.method_for_largest = SIXEL_LARGE_NORM;
    request.method_for_rep = SIXEL_REP_AVERAGE_PIXELS;
    request.quality_mode = SIXEL_QUALITY_HIGH;
    request.force_palette = 0;
    request.use_reversible = 0;
    request.quantize_model = SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.prefer_float32 = 0;
    if (temporal_data != NULL) {
        request.temporal_reference_data = temporal_data;
        request.temporal_reference_length = data_size;
        request.temporal_reference_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        request.temporal_match_weight = 8U;
    }

    status = palette->vtbl->generate(palette, &request);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = palette->vtbl->get_entries(palette, &entries_view);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (entries_view.entries == NULL ||
            entries_view.entry_count != 1U ||
            entries_view.depth != 3 ||
            entries_view.entries_size < 3U) {
        goto end;
    }

    out[0] = entries_view.entries[0];
    out[1] = entries_view.entries[1];
    out[2] = entries_view.entries[2];
    ok = 1;

end:
    if (object != NULL) {
        ((sixel_palette_t *)object)->vtbl->unref((sixel_palette_t *)object);
    }
    if (palette != NULL && palette->vtbl != NULL &&
            palette->vtbl->unref != NULL) {
        palette->vtbl->unref(palette);
    }
    if (factory != NULL && factory->vtbl != NULL &&
            factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok;
}

static int
test_palette_temporal_reference_same_offset_weight(void)
{
    unsigned char current[27];
    unsigned char previous_same_offset[27];
    unsigned char previous_shifted[27];
    unsigned char baseline[3];
    unsigned char weighted[3];
    unsigned char shifted[3];
    size_t i;
    size_t offset;

    memset(current, 0, sizeof(current));
    memset(previous_same_offset, 0, sizeof(previous_same_offset));
    memset(previous_shifted, 0, sizeof(previous_shifted));
    memset(baseline, 0, sizeof(baseline));
    memset(weighted, 0, sizeof(weighted));
    memset(shifted, 0, sizeof(shifted));
    i = 0U;
    offset = 0U;

    for (i = 0U; i < 8U; ++i) {
        offset = i * 3U;
        previous_same_offset[offset + 0U] = 255U;
        previous_same_offset[offset + 1U] = 0U;
        previous_same_offset[offset + 2U] = 0U;
        previous_shifted[offset + 0U] = 255U;
        previous_shifted[offset + 1U] = 0U;
        previous_shifted[offset + 2U] = 0U;
    }

    current[24] = 255U;
    current[25] = 255U;
    current[26] = 255U;
    previous_same_offset[24] = 255U;
    previous_same_offset[25] = 255U;
    previous_same_offset[26] = 255U;
    previous_shifted[0] = 255U;
    previous_shifted[1] = 255U;
    previous_shifted[2] = 255U;
    previous_shifted[24] = 255U;
    previous_shifted[25] = 0U;
    previous_shifted[26] = 0U;

    if (!generate_single_average(current,
                                 (unsigned int)sizeof(current),
                                 NULL,
                                 baseline)) {
        return 0;
    }
    if (!generate_single_average(current,
                                 (unsigned int)sizeof(current),
                                 previous_same_offset,
                                 weighted)) {
        return 0;
    }
    if (!generate_single_average(current,
                                 (unsigned int)sizeof(current),
                                 previous_shifted,
                                 shifted)) {
        return 0;
    }
    if (weighted[0] <= baseline[0] + 64U ||
            weighted[1] <= baseline[1] + 64U ||
            weighted[2] <= baseline[2] + 64U) {
        return 0;
    }
    if (memcmp(baseline, shifted, sizeof(baseline)) != 0) {
        return 0;
    }

    return 1;
}

int
test_palette_0005_palette_temporal_reference(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_palette_temporal_reference_same_offset_weight()) {
        fprintf(stderr, "palette temporal reference weighting failed\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
