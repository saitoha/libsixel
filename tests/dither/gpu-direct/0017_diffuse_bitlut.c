/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

/* GPU error diffusion through a 5bit/6bit cell-center table, on Metal
 * (injected program) or Vulkan (device-less table). A deadzone above every
 * possible correction discards all carry, so each index is the fractional
 * bit LUT reference of its source pixel. Live carry with history
 * reproduces the frozen indexes through both the monolithic run and the
 * deferred solve and resolve. Indexes are one digit per pixel of the 19x17
 * gray fixture, serpentine against the offset baseline; Metal and Vulkan
 * produced identical indexes when these were recorded. */

typedef struct direct_bitlut_case {
    int kind;
    char const *fs;
    char const *lso2;
} direct_bitlut_case_t;

static direct_bitlut_case_t const direct_bitlut_cases[] = {
    { SIXEL_LUT_POLICY_5BIT,
      "0001010111111212222"
      "0001100111111112222"
      "0100111111121222222"
      "0001111111112122222"
      "0011111112221222224"
      "0111111121122222444"
      "0111111211212224444"
      "1111121121222444444"
      "1111122122222444444"
      "1111211222224442444"
      "1112122222244444244"
      "1111222222444444434"
      "1112212222444444444"
      "2121222244442434444"
      "2122222244334333433"
      "2222224444443344343"
      "1222244442434344444",
      "0001010111111212222"
      "0000010111111112222"
      "0100111111122122222"
      "0001111111111222222"
      "0011111111221222224"
      "0111111112122222444"
      "0111111212211222444"
      "1111111112222244444"
      "1111121222222244443"
      "1111112222224444434"
      "1111112122244444343"
      "1111212224444443344"
      "1122122224444434434"
      "1112122244444444343"
      "2122222244444344433"
      "1212244444444444333"
      "2222224444443433444" },
    { SIXEL_LUT_POLICY_6BIT,
      "0000101111111121122"
      "0000011111111121222"
      "0000001111121222222"
      "1001111111122222222"
      "1111111112112222224"
      "0011111112222222444"
      "1111111222222222444"
      "1111112212222444444"
      "1111121222222444444"
      "1112212122224444444"
      "1111222222244434434"
      "1222122222444444444"
      "1222222224444443333"
      "2112222444442443344"
      "2222222444344434443"
      "2122224444424343443"
      "2122224442243444334",
      "0000010111111112122"
      "0000101111111121222"
      "0000111111111222222"
      "0101011111122122222"
      "0110111111122222224"
      "0101111111222222244"
      "1111111121222224444"
      "1111112212222444444"
      "1111121122222444444"
      "1111112122244444444"
      "1111112222244444434"
      "1112112222444444443"
      "1122122222444434344"
      "1112222244444344444"
      "1212222244444434333"
      "2122224444444343443"
      "2122224444343344332" }
};

static int
direct_bitlut_solve(direct_bitlut_case_t const *c, int method,
    double deadzone, int history, int deferred, unsigned char *indexes,
    unsigned char *reference)
{
    static unsigned char const colors[12] = {
        0,0,0, 13,13,13, 26,26,26, 51,51,51
    };
    lso2_gpu_fixture_t f;
    sixel_lookup_policy_configuration_t configuration;
    double q[3];
    int outcome, i;

    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = c->kind;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    outcome = lso2_gpu_fixture_open_lookup(&f, method, &configuration);
    if (outcome != 0) goto end;
    outcome = 1;
    f.configuration.options.diffusion_deadzone = deadzone;
    if (history) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 1) == SIXEL_OK);
    }
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    if (deferred) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_deferred(&f) == SIXEL_OK);
    } else {
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    }
    memcpy(indexes, f.image->indexes, LSO2_GPU_PIXELS);
    if (reference != NULL) {
        /* The reserved fifth entry is the key and never answers. */
        for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
            q[0] = f.pixels[3 * i];
            q[1] = f.pixels[3 * i + 1];
            q[2] = f.pixels[3 * i + 2];
            reference[i] = (unsigned char)lso2_gpu_fractional_reference(
                &configuration, colors, 4, q);
        }
    }
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}

static int
direct_bitlut_frozen(unsigned char const *indexes, char const *expected)
{
    int i;

    for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
        if (indexes[i] != (unsigned char)(expected[i] - '0')) {
            return 0;
        }
    }
    return 1;
}

int
test_gpu_direct_0017_diffuse_bitlut(int argc, char **argv)
{
    unsigned char plain[LSO2_GPU_PIXELS], reference[LSO2_GPU_PIXELS];
    unsigned char run[LSO2_GPU_PIXELS], deferred[LSO2_GPU_PIXELS];
    direct_bitlut_case_t const *c;
    size_t n;
    int k, method, outcome;

    (void)argc;
    (void)argv;
    for (n = 0; n < sizeof(direct_bitlut_cases) /
            sizeof(*direct_bitlut_cases); ++n) {
        c = &direct_bitlut_cases[n];
        for (k = 0; k < 2; ++k) {
            method = k == 0 ? SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_LSO2;
            /* LSO2 has no deadzone; the fixed-tap limiter has one. */
            if (method == SIXEL_DIFFUSE_FS) {
                outcome = direct_bitlut_solve(c, method, 1.0e6, 0, 0, plain,
                    reference);
                if (outcome != 0) return outcome;
                if (memcmp(plain, reference, LSO2_GPU_PIXELS) != 0) {
                    fprintf(stderr, "direct bitlut: kind %d lookup "
                        "mismatch\n", c->kind);
                    return 1;
                }
            }
            outcome = direct_bitlut_solve(c, method, 0.0, 1, 0, run, NULL);
            if (outcome != 0) return outcome;
            outcome = direct_bitlut_solve(c, method, 0.0, 1, 1, deferred,
                NULL);
            if (outcome != 0) return outcome;
            if (memcmp(run, deferred, LSO2_GPU_PIXELS) != 0 ||
                    !direct_bitlut_frozen(run, k == 0 ? c->fs : c->lso2)) {
                fprintf(stderr, "direct bitlut: kind %d method %d "
                    "history mismatch\n", c->kind, method);
                return 1;
            }
        }
    }
    return 0;
}
