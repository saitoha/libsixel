/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/progressive-predict.h"
#include "src/progressive-apply.h"

#define PREDICT_CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "prediction assertion at line %d\n", __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_predict_case(int mode)
{
    sixel_repair_predict_model_t *model;
    sixel_repair_predict_model_t *reverse;
    sixel_repair_prediction_t result;
    sixel_repair_prediction_t reversed;
    sixel_repair_application_t application;
    sixel_palette_emitted_color_t colors[2];
    sixel_repair_work_t work;
    unsigned int slots[2] = {0, 1};
    float const palette[2][3] = {{0.0f, 0, 0}, {1.0f, 0, 0}};
    float target[3] = {0.5f, 0, 0};
    float source[384];
    double errors[128];
    double gains[128];
    unsigned char selected[128];
    unsigned char indices[128];
    unsigned char paint[128];
    size_t population;
    size_t samples;
    size_t minimum;
    size_t maximum_position;
    uint64_t hash;
    double gain;
    double maximum;
    double excess;
    int band;
    int done;
    int i;
    int j;
    int ok;

    model = calloc(1, sizeof(*model));
    reverse = calloc(1, sizeof(*reverse));
    ok = 0;
    PREDICT_CHECK(model != NULL && reverse != NULL);
    sixel_repair_predict_reset(model, 0.25);
    if (mode == 124) {
        /* The same target has different retained errors. One is already
         * better than the candidate and must not be charged its distance. */
        sixel_repair_predict_add(model, 0, target, 0.125);
        sixel_repair_predict_add(model, 1, target, 0.5);
        sixel_repair_predict_add(model, 2, target, 1.0);
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.25, 256,
            &result) == SIXEL_OK);
        PREDICT_CHECK(result.exact && result.samples == 3 &&
            result.gain == 1 && result.excess_gain == 1 &&
            result.selected == 2 && result.painted == 2 &&
            result.resolved == 2 && result.maximum_lower == 0.25 &&
            result.maximum_upper == 0.25);
    } else if (mode == 125) {
        sixel_repair_predict_reset(model, 0);
        sixel_repair_predict_add(model, 0, palette[0], 0);
        sixel_repair_predict_add(model, 1, palette[0], 1e-13);
        sixel_repair_predict_add(model, 2, target, 0.5);
        sixel_repair_predict_add(model, 3, target, 0.499);
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.5, 256,
            &result) == SIXEL_OK && result.selected == 1 &&
            result.gain == 0.25 && result.maximum_upper == 0.499);
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0, 256,
            &result) == SIXEL_OK && result.painted == 2 &&
            result.resolved == 0 && result.selected == 3);
        PREDICT_CHECK(sixel_repair_predict(model, NULL, 0, 0, 256,
            &result) == SIXEL_OK && result.gain == 0 &&
            result.maximum_lower == 0.5 && result.maximum_upper == 0.5);
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, NAN, 256,
            &result) == SIXEL_BAD_ARGUMENT);
    } else if (mode == 126 || mode == 127) {
        sixel_repair_predict_reset(reverse, 0.25);
        for (i = 0; i < 10000; ++i) {
            target[0] = mode == 126 ? 0.5f : (i % 61) / 64.0f;
            sixel_repair_predict_add(model, (size_t)i, target, 1);
            target[0] = mode == 126 ? 0.5f : ((9999 - i) % 61) / 64.0f;
            sixel_repair_predict_add(reverse, (size_t)(9999 - i), target, 1);
        }
        population = samples = 0;
        for (band = 0; band < SIXEL_REPAIR_PREDICT_BANDS; ++band) {
            population += model->bands[band].population;
            samples += model->bands[band].count;
        }
        PREDICT_CHECK(population == 10000 && samples == 256);
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.25, 256,
            &result) == SIXEL_OK && !result.exact);
        if (mode == 126) {
            PREDICT_CHECK(result.gain == 7500 && result.painted == 10000 &&
                result.excess_gain == 7500 && result.resolved == 10000);
            PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.25, 0,
                &result) == SIXEL_OK && result.samples == 256 &&
                result.gain == 7500);
            sixel_repair_predict_reset(reverse, 0.25);
            for (i = 0; i < 1510; ++i) {
                sixel_repair_predict_add(reverse, (size_t)i, target,
                    i < 500 ? 0.125 : i < 510 ? 0.5 : 8);
            }
            PREDICT_CHECK(sixel_repair_predict(reverse, palette, 1, 0.25,
                0, &result) == SIXEL_OK && result.samples == 256 &&
                fabs(result.gain - 7752.5) < 1e-8);
        } else {
            PREDICT_CHECK(sixel_repair_predict(reverse, palette, 1, 0.25,
                256, &reversed) == SIXEL_OK &&
                result.gain == reversed.gain &&
                result.excess_gain == reversed.excess_gain);
            PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.25, 16,
                &result) == SIXEL_OK &&
                sixel_repair_predict(reverse, palette, 1, 0.25, 16,
                &reversed) == SIXEL_OK && result.gain == reversed.gain);
        }
    } else if (mode == 128) {
        for (i = 0; i < 10; ++i) {
            sixel_repair_predict_add(model, (size_t)i, palette[0], 0.5);
        }
        band = sixel_repair_predict_band(model, 0.5);
        minimum = 0;
        hash = UINT64_MAX;
        for (i = 0; i < 10; ++i) {
            if (model->bands[band].points[i].priority < hash) {
                hash = model->bands[band].points[i].priority;
                minimum = model->bands[band].points[i].position;
            }
        }
        maximum_position = (minimum + 1) % 10;
        sixel_repair_predict_reset(model, 0.25);
        for (i = 0; i < 10; ++i) {
            target[0] = (size_t)i == maximum_position ? 2.0f : 0;
            sixel_repair_predict_add(model, (size_t)i, target, 0.5);
        }
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.25, 1,
            &result) == SIXEL_OK && !result.exact &&
            result.maximum_lower == 0 && result.maximum_upper == 0.5);
        PREDICT_CHECK(sixel_repair_predict(model, palette, 1, 0.25, 256,
            &result) == SIXEL_OK && result.exact &&
            result.maximum_lower == 0.5 && result.maximum_upper == 0.5);
    } else if (mode == 129) {
        memset(&application, 0, sizeof(application));
        memset(colors, 0, sizeof(colors));
        memcpy(colors[0].working, palette[0], sizeof(palette[0]));
        memcpy(colors[1].working, palette[1], sizeof(palette[1]));
        for (i = 0; i < 128; ++i) {
            source[i * 3] = (i % 17 - 4) / 8.0f;
            source[i * 3 + 1] = source[i * 3 + 2] = 0;
            errors[i] = (i % 7) * 0.125;
            selected[i] = errors[i] > 0.25 && errors[i] >= 0.5;
            sixel_repair_predict_add(model, (size_t)i, source + i * 3,
                errors[i]);
        }
        application.source = source;
        application.errors = errors;
        application.selected = selected;
        application.colors = colors;
        application.palette = slots;
        application.indices = indices;
        application.paint = paint;
        application.gains = gains;
        application.count = 128;
        application.width = 16;
        application.color_count = 2;
        PREDICT_CHECK(sixel_repair_application_begin(&application) ==
            SIXEL_OK);
        done = 0;
        for (j = 0; !done && j < 1000; ++j) {
            memset(&work, 0, sizeof(work));
            work.pixel_limit = work.distance_limit = 5;
            PREDICT_CHECK(sixel_repair_application_advance(&application,
                &work, &done) == SIXEL_OK);
        }
        PREDICT_CHECK(done);
        gain = excess = maximum = 0;
        for (i = 0; i < 128; ++i) {
            gain += gains[i];
            excess += fmax(0, errors[i] - 0.25) -
                fmax(0, errors[i] - gains[i] - 0.25);
            maximum = fmax(maximum, errors[i] - gains[i]);
        }
        PREDICT_CHECK(sixel_repair_predict(model, palette, 2, 0.5, 256,
            &result) == SIXEL_OK && result.exact &&
            fabs(result.gain - gain) < 1e-12 &&
            fabs(result.excess_gain - excess) < 1e-12 &&
            result.painted == application.painted &&
            result.maximum_lower == maximum &&
            result.maximum_upper == maximum);
    }
    ok = 1;
end:
    free(model);
    free(reverse);
    return ok ? 0 : 1;
}
