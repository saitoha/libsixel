/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <string.h>

#include "palette-sample.h"
#include "filter-palette-colors.h"
#include "filter-factory-palette-colors.h"

SIXELSTATUS
sixel_palette_prepare_sample_statistics(void const *data, size_t length,
    int pixelformat, int colorspace, int alpha_zero_is_transparent,
    sixel_sample_statistics_t **out, sixel_allocator_t *allocator,
    sixel_timeline_logger_t *logger)
{
    sixel_filter_palette_colors_config_t config;
    sixel_frame_transparency_t transparency;
    sixel_sample_stream_t input;
    sixel_sample_stream_t output;
    sixel_filter_t *filter;
    SIXELSTATUS status;

    if (out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *out = NULL;
    filter = NULL;
    memset(&config, 0, sizeof(config));
    memset(&transparency, 0, sizeof(transparency));
    config.collect_statistics = 1;
    transparency.alpha_zero_is_transparent = alpha_zero_is_transparent;
    sixel_sample_stream_init(&input);
    sixel_sample_stream_init(&output);
    status = sixel_sample_stream_bind_borrowed_buffer(&input, data, length,
        pixelformat, colorspace, &transparency,
        SIXEL_PALETTE_SAMPLING_FULL_FRAME,
        SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_filter_factory_create_palette_colors(&config, &filter);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_filter_bind_sample_input(filter, &input);
    sixel_filter_bind_sample_output(filter, &output, pixelformat, colorspace);
    status = sixel_filter_run(filter, allocator, logger);
    if (SIXEL_SUCCEEDED(status)) {
        *out = output.statistics;
        output.statistics = NULL;
    }
end:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&output);
    sixel_sample_stream_dispose(&input);
    return status;
}

sixel_sample_statistics_t *
sixel_palette_sample_statistics_for(sixel_palette_build_attempt_t *attempt,
    void const *data, size_t length, int pixelformat)
{
    if (attempt == NULL || attempt->statistics_data != data ||
            attempt->statistics_length != length ||
            attempt->statistics_pixelformat != pixelformat) {
        return NULL;
    }
    return attempt->statistics;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
