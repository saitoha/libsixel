/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"

/* GPU error diffusion through an FHEDT lookup, on Metal or Vulkan, each
 * composing the policy's own lookup program. A deadzone above every possible
 * correction discards all carry, so each index is the fractional FHEDT
 * reference of its source pixel. Live carry with history reproduces the
 * frozen indexes through both the monolithic run and the deferred solve and
 * resolve. Indexes are one digit per pixel of the 19x17 gray fixture,
 * serpentine against the offset baseline; Metal and Vulkan produced
 * identical indexes when these were recorded. */

typedef struct direct_fhedt_case {
    int resolution;
    int refine;
    int dist2;
    char const *fs;
    char const *lso2;
} direct_fhedt_case_t;

static direct_fhedt_case_t const direct_fhedt_cases[] = {
    { 64, 0, 0,
      "0000101111111211122"
      "0001101111112121222"
      "0110111111121222222"
      "1001111111112122222"
      "0111111112111222224"
      "1011111121211222444"
      "1111111212222222444"
      "1111111122222444444"
      "1111121222222444424"
      "1111212222224244444"
      "1112222222224444434"
      "1121221222444444444"
      "1122222222444444443"
      "2121222244442443344"
      "1222222444444434334"
      "2222224444444444344"
      "2222244444244344344",
      "0000010111111121222"
      "0001011111112212122"
      "0110101111112122222"
      "0001111111121222222"
      "0111111111222222224"
      "1011111112122222444"
      "1111111212222222444"
      "1111111221222244444"
      "1111111122222244443"
      "1111211222224444444"
      "1112122222244444443"
      "1111222222442444434"
      "1121222222444444343"
      "1121222244444444443"
      "2122222244444334333"
      "1222224444444443343"
      "2222224444443434332" },
    { 64, 1, 1,
      "0000101111111212222"
      "0010011111111112222"
      "0110111111122222222"
      "0101111112211222222"
      "0011111111121222224"
      "1111111121212222444"
      "1111111122222222444"
      "1111112212222242444"
      "1111111222222244444"
      "1111212122224444444"
      "1112112222244444444"
      "1221212222444423344"
      "1122222222444444444"
      "1222222244442434444"
      "1121222244244444343"
      "2122224444444443444"
      "2222224442444344442",
      "0000101111111212222"
      "0010011111111112222"
      "0000111111111222222"
      "0001111111112222222"
      "0011111112212222222"
      "1111111111122222244"
      "1111111112122222444"
      "1111112212222244244"
      "1111121222222244444"
      "1111212122224444444"
      "1111122222244444444"
      "1112212222444444443"
      "1212222222444443344"
      "2122222244444443444"
      "2222222244444334343"
      "2122224444444444433"
      "2122224444443434442" },
    { 128, 1, 0,
      "0000101111111121222"
      "0010011111112112122"
      "0110101111121212222"
      "0101111111112122222"
      "0011111112112222224"
      "1111111111212222444"
      "0111111112222222444"
      "1111112122122244444"
      "1111121212222244444"
      "1111212122224444444"
      "1112112222244434434"
      "1121222222444444444"
      "1122122222444443343"
      "2121222244442434444"
      "1212222244344434333"
      "1222224444444344343"
      "2222224442244344344",
      "0000101111111121222"
      "0010011111112121222"
      "0000111111111222222"
      "0001111111121222222"
      "0111111111122222224"
      "0101111111212222244"
      "1011111212222222444"
      "1111111112222244444"
      "1111111222222244444"
      "1111212122244444444"
      "1112122222224444344"
      "1111212222442444443"
      "1121222222444444344"
      "2121222244444444444"
      "1212222244444334433"
      "1222224444444444333"
      "2222224444443433434" },
    { 256, 1, 0,
      "0000101111111121222"
      "0010010111112121122"
      "0010111111121212222"
      "0101111111112122222"
      "0011111112112222224"
      "0111111111222222444"
      "1111111212122222444"
      "1111112122222444444"
      "1111121222222444444"
      "1111212222224444444"
      "1112122122244434434"
      "1121221222444444444"
      "1212222222444443333"
      "2121222244442443344"
      "1212222244344434333"
      "1222224444444344343"
      "2222224444244344344",
      "0000101111111121222"
      "0010010111112221122"
      "0000111111111222222"
      "0101111111212222222"
      "0101111111121222224"
      "0111111111212222244"
      "1011111212222222444"
      "1111112121122244444"
      "1111111222222244444"
      "1111112222244444444"
      "1111222122244444434"
      "1121222224444444443"
      "1121222224444443344"
      "1121222244444444444"
      "1222222244444334333"
      "2222224444444444343"
      "2222224444443433434" },
    { 256, 0, 0,
      "0000101111111121222"
      "0010010111112121122"
      "0010111111121212222"
      "0101111111112122222"
      "0011111112112222224"
      "0111111111222222444"
      "1111111212122222444"
      "1111112122222444444"
      "1111121222222444444"
      "1111212222224444444"
      "1112122122244434434"
      "1121221222444444444"
      "1212222222444443333"
      "2121222244442443344"
      "1212222244344434333"
      "1222224444444344343"
      "2222224444244344344",
      "0000101111111121222"
      "0010010111112221122"
      "0000111111111222222"
      "0101111111122222222"
      "0101111111122222224"
      "0111111111212222244"
      "1011111212222222444"
      "1111112112222244444"
      "1111112122222244443"
      "1111121222244444444"
      "1111222222244444443"
      "1111222224444444444"
      "1121222224444443344"
      "1121222244444444344"
      "2122222244444334433"
      "1122224444443444333"
      "2222224444343433434" }
};

static void
direct_fhedt_configuration(direct_fhedt_case_t const *c,
    sixel_lookup_policy_configuration_t *configuration)
{
    memset(configuration, 0, sizeof(*configuration));
    configuration->kind = SIXEL_LUT_POLICY_FHEDT;
    configuration->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    configuration->fhedt_resolution = c->resolution;
    configuration->fhedt_refine = c->refine;
    configuration->fhedt_use_dist2 = c->dist2;
    configuration->fhedt_weights[0] = 1;
    configuration->fhedt_weights[1] = 1;
    configuration->fhedt_weights[2] = 1;
}

static int
direct_fhedt_solve(direct_fhedt_case_t const *c, int method,
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

    direct_fhedt_configuration(c, &configuration);
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
direct_fhedt_frozen(unsigned char const *indexes, char const *expected)
{
    int i;

    for (i = 0; i < LSO2_GPU_PIXELS; ++i) {
        if (indexes[i] != (unsigned char)(expected[i] - '0')) {
            return 0;
        }
    }
    return 1;
}

/* A case observes either the deadzone lookup oracle or one history result.
 * Both execution forms have their own frozen-reference assertion. */
int
test_direct_fhedt_case(int setting, int method, int observation)
{
    unsigned char actual[LSO2_GPU_PIXELS], reference[LSO2_GPU_PIXELS];
    direct_fhedt_case_t const *c;
    int outcome;

    c = &direct_fhedt_cases[setting];
    outcome = direct_fhedt_solve(c, method,
        observation == 0 ? 1.0e6 : 0.0, observation != 0,
        observation == 2, actual, observation == 0 ? reference : NULL);
    if (outcome != 0) return outcome;
    if (observation == 0) {
        return memcmp(actual, reference, LSO2_GPU_PIXELS) != 0;
    }
    return !direct_fhedt_frozen(actual,
        method == SIXEL_DIFFUSE_FS ? c->fs : c->lso2);
}
