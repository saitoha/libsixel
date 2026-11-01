/* SPDX-License-Identifier: MIT */
/* The real parser and writer satisfy separate integer and byte contracts. */
#include <stdint.h>
#include <string.h>
#include "src/cms-curv.h"

int
test_cms_0146(int argc, char **argv)
{
    unsigned char tag[17] = "curv\0\0\0\0\0\0\0\1";
    unsigned char bad[17];
    unsigned char encoded[4];
    unsigned char expected[4];
    uint32_t gamma;
    unsigned int word;
    unsigned int i;
    size_t length;

    (void)argc;
    (void)argv;
    for (word = 1u; word <= 65535u; ++word) {
        tag[12] = (unsigned char)(word / 256u);
        tag[13] = (unsigned char)(word % 256u);
        /* Scaling the real exponent by 65536 is exact here. This oracle
         * uses division/multiplication, not the production bit shifts. */
        gamma = 0u;
        if (!sixel_cms_curv_gamma(tag, 14u, &gamma) ||
            (double)gamma / 65536.0 != (double)word / 256.0) {
            return 1;
        }
        gamma = 0u;
        if (!sixel_cms_curv_gamma(tag, 16u, &gamma) ||
            gamma != word * 256u) {
            return 1;
        }
        /* Verify the writer independently, not just a parser roundtrip. */
        expected[0] = 0u;
        expected[1] = tag[12];
        expected[2] = tag[13];
        expected[3] = 0u;
        sixel_cms_write_gamma(encoded, word * 256u);
        if (memcmp(encoded, expected, sizeof(encoded)) != 0) {
            return 1;
        }
    }
    for (length = 0u; length <= sizeof(tag); ++length) {
        if (length == 14u || length == 16u) {
            continue;
        }
        gamma = 123u;
        if (sixel_cms_curv_gamma(tag, length, &gamma) || gamma != 123u) {
            return 1;
        }
    }
    /* Each signature, reserved, count, and padding byte is significant. */
    for (i = 0u; i < 16u; ++i) {
        if (i == 12u || i == 13u) {
            continue;
        }
        memcpy(bad, tag, sizeof(bad));
        bad[i] ^= 1u;
        gamma = 123u;
        if (sixel_cms_curv_gamma(bad, 16u, &gamma) || gamma != 123u) {
            return 1;
        }
    }
    tag[12] = tag[13] = 0u;
    gamma = 123u;
    if (sixel_cms_curv_gamma(tag, 14u, &gamma) || gamma != 123u ||
        sixel_cms_curv_gamma(tag, 16u, &gamma) || gamma != 123u ||
        sixel_cms_curv_gamma(NULL, 14u, &gamma) || gamma != 123u ||
        sixel_cms_curv_gamma(tag, 14u, NULL)) {
        return 1;
    }
    return 0;
}
