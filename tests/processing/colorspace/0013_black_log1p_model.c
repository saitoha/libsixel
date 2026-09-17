/* SPDX-License-Identifier: MIT */
/* Check the runtime libm assumption used by black-lightness error bounds. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tests/data/color-numeric/black-lightness.h"

int
test_color_0013(int argc, char **argv)
{
    volatile double input;
    double decoded;
    double actual;
    size_t i;

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(color_black_log_probes) /
                    sizeof(color_black_log_probes[0]); ++i) {
        memcpy(&decoded, &color_black_log_probes[i].bits, sizeof(decoded));
        /* Volatile input prevents compile-time evaluation of the libm call.
         * Decimal-derived bounds cover both signs of the cancellation
         * witness and the model endpoint. Signed zero is observed directly. */
        input = decoded;
        actual = log1p(input);
        if (!isfinite(actual) || actual < color_black_log_probes[i].lo ||
            actual > color_black_log_probes[i].hi ||
            (decoded == 0 && !!signbit(actual) != !!signbit(decoded))) {
            fprintf(stderr, "black log1p probe %zu: %.17g\n", i, actual);
            return 1;
        }
    }
    return 0;
}
