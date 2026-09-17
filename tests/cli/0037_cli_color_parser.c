/*
 * SPDX-License-Identifier: MIT
 *
 * Literal protocol witnesses for the CLI-owned color reply recognizer.
 * Each TAP wrapper selects one observation. Every short witness is split
 * at every byte boundary, so no test depends on a whole read packet.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "converters/terminal-color.h"

#define CLI_COLOR_DCS "\033P"
#define CLI_COLOR_ST "\033\\"
#define CLI_COLOR_P3 CLI_COLOR_DCS "1;1;1;1;4./r" CLI_COLOR_ST

typedef struct cli_color_case {
    char const *group;
    img2sixel_color_query_t query;
    char const *bytes;
    img2sixel_color_result_t result;
    unsigned long s2;
    unsigned long s3;
    unsigned long definition;
    unsigned long policy;
} cli_color_case_t;

static cli_color_case_t const cli_color_cases[] = {
    { "p3", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "defaults", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 0ul, 0ul, 3ul, 0ul },
    { "defaults", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 0ul, 0ul, 3ul, 0ul },
    { "defaults", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 0ul, 0ul, 3ul, 0ul },
    { "defaults", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 0ul, 0ul, 3ul, 0ul },
    { "unavailable", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;0;0;0;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNAVAILABLE, 0ul, 0ul, 3ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;0;1;0;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;0;0;1;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;0;0;0;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;0;0;0;3./rX" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0;1./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;0;3./rX" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "consistency", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;4./rX" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "s3-only", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;0;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 0ul, 1ul, 4ul, 0ul },
    { "unsupported", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;5./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 0ul, 5ul, 0ul },
    { "unsupported", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;2./runsupported" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 0ul, 2ul, 0ul },
    { "unsupported", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;2147483647./runsupported" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 0ul, 2147483647ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;2;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;2;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;2;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;4;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS ";1;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "?1;1;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;+4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;-4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;4:0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;2147483648./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "2147483648;1;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "numeric", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "01;01;01;01;04./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "policy", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;0./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_SAVED_POLICY, 0ul, 0ul, 0ul, 0ul },
    { "policy", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;1./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_SAVED_POLICY, 0ul, 0ul, 0ul, 1ul },
    { "policy", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;2./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_SAVED_POLICY, 0ul, 0ul, 0ul, 2ul },
    { "policy", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;3./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_SAVED_POLICY, 0ul, 0ul, 0ul, 3ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "0$r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_POLICY_FAILURE, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "0$r1;2./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "2$r1;2./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;4./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;2;0./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;2147483648./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "policy-failure", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_DCS "1$r1;+2./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "q1;1;1;1;4./r" CLI_COLOR_ST
        CLI_COLOR_DCS "2;1;1;1;4./r" CLI_COLOR_ST
        CLI_COLOR_DCS "0$r" CLI_COLOR_ST CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_POLICY,
        CLI_COLOR_P3 CLI_COLOR_DCS "1$r0m" CLI_COLOR_ST
        CLI_COLOR_DCS "1$r2;2./p" CLI_COLOR_ST
        CLI_COLOR_DCS "1$r1;3./p" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_SAVED_POLICY, 0ul, 0ul, 0ul, 3ul },
    { "c1", IMG2SIXEL_COLOR_CAPABILITY,
        "\233?1./r\2201;1;1;1;4./r\234",
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "c1", IMG2SIXEL_COLOR_CAPABILITY,
        "\033[?1./r\2201;1;1;1;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "c1", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;4./r\234",
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "c1", IMG2SIXEL_COLOR_POLICY,
        "\2201$r1;0./p\234",
        IMG2SIXEL_COLOR_SAVED_POLICY, 0ul, 0ul, 0ul, 0ul },
    { "cancel", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;3./r\030" CLI_COLOR_ST
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "cancel", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;3./r\032" CLI_COLOR_ST
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "cancel", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;3./r\033[0m" CLI_COLOR_ST
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "cancel", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;3./r\033\033[0m" CLI_COLOR_ST
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "cancel", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;3./r" CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;260000;2;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;0;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;2;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;1./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-definition", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;2147483647;3;2./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_UNSUPPORTED, 1ul, 1ul, 1ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;;2;3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;2;3;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;2;4./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;2147483648./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;2147483648./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;2;2147483648./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;+220000./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000;2:3./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;1;1;220000./rx" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "simple-invalid", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;1./runsupported" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "icc-empty", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;2./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;0;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;0;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;2;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;2;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;3;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;3;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;4;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;4;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;5;./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;5;0./r" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;0./rx" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;3./rx" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;4./rx" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    { "common-shape", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "1;1;1;0;5./rx" CLI_COLOR_ST,
        IMG2SIXEL_COLOR_INVALID, 0ul, 0ul, 0ul, 0ul },
    /*
     * Literal selector-like payload is never a reply. ESC P, in contrast,
     * cancels an old string and starts a new DCS under this reader contract.
     */
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033Pq1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\033\\"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033Pq1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\234"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033]0;1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\007"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033]0;1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\033\\"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\2350;1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\234"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033_1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\033\\"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033^1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\033\\"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033X1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\033\\"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\2371;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\234"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\2361;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\234"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\2301;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\234"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033]0;1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\030plain\033\\"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "unrelated", IMG2SIXEL_COLOR_CAPABILITY,
        "\033_1;1;1;1;4./r 1$r1;2./p 1;1;1;0;3./r\032plain\234"
        CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
    { "cancel", IMG2SIXEL_COLOR_CAPABILITY,
        CLI_COLOR_DCS "q1;1;1;1;4./r" CLI_COLOR_P3,
        IMG2SIXEL_COLOR_AVAILABLE, 1ul, 1ul, 4ul, 0ul },
};

static int
cli_color_run_case(cli_color_case_t const *test)
{
    img2sixel_color_parser_t parser;
    img2sixel_color_result_t result;
    size_t size;
    size_t split;
    size_t consumed;
    size_t first;

    size = strlen(test->bytes);
    for (split = 0u; split <= size; ++split) {
        img2sixel_color_init(&parser, test->query);
        result = img2sixel_color_feed(&parser,
            (unsigned char const *)test->bytes, split, &first);
        if (first != split ||
            (split < size && result != IMG2SIXEL_COLOR_PENDING)) {
            fprintf(stderr, "%s: premature result at %zu/%zu\n",
                    test->group, split, size);
            return EXIT_FAILURE;
        }
        result = img2sixel_color_feed(&parser,
            (unsigned char const *)test->bytes + split,
            size - split, &consumed);
        if (consumed != size - split || result != test->result ||
            parser.s2 != test->s2 || parser.s3 != test->s3 ||
            parser.definition != test->definition ||
            parser.policy != test->policy ||
            img2sixel_color_incomplete(&parser)) {
            fprintf(stderr, "%s: result %d at %zu/%zu, expected %d\n",
                    test->group, (int)result, split, size,
                    (int)test->result);
            return EXIT_FAILURE;
        }
    }
    /* A byte-at-a-time feed also crosses several boundaries per reply. */
    img2sixel_color_init(&parser, test->query);
    for (split = 0u; split < size; ++split) {
        result = img2sixel_color_feed(&parser,
            (unsigned char const *)test->bytes + split, 1u, &consumed);
        if (consumed != 1u ||
            result != (split + 1u == size ? test->result
                                         : IMG2SIXEL_COLOR_PENDING)) {
            fprintf(stderr, "%s: byte feed failed at %zu\n",
                    test->group, split);
            return EXIT_FAILURE;
        }
    }
    if (parser.s2 != test->s2 || parser.s3 != test->s3 ||
        parser.definition != test->definition ||
        parser.policy != test->policy) {
        fprintf(stderr, "%s: byte feed changed values\n", test->group);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int
cli_color_truncated(void)
{
    img2sixel_color_parser_t parser;
    char const *witnesses[] = {
        CLI_COLOR_P3, CLI_COLOR_DCS "1$r1;2./p" CLI_COLOR_ST,
        CLI_COLOR_DCS "1;1;1;0;2./rpayload" CLI_COLOR_ST
    };
    size_t i;
    size_t prefix;
    size_t consumed;

    for (i = 0u; i < sizeof(witnesses) / sizeof(witnesses[0]); ++i) {
        for (prefix = 1u; prefix < strlen(witnesses[i]); ++prefix) {
            img2sixel_color_init(&parser, i == 1u ? IMG2SIXEL_COLOR_POLICY
                                                : IMG2SIXEL_COLOR_CAPABILITY);
            if (img2sixel_color_feed(&parser,
                    (unsigned char const *)witnesses[i], prefix,
                    &consumed) != IMG2SIXEL_COLOR_PENDING ||
                consumed != prefix || !img2sixel_color_incomplete(&parser)) {
                fprintf(stderr, "truncated reply accepted at %zu\n", prefix);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

static int
cli_color_long_payload(int saturate)
{
    img2sixel_color_parser_t parser;
    unsigned char block[1024];
    size_t consumed;
    size_t i;
    size_t expected;
    char const *header;

    header = CLI_COLOR_DCS "1;1;1;0;2./r";
    memset(block, 'x', sizeof(block));
    img2sixel_color_init(&parser, IMG2SIXEL_COLOR_CAPABILITY);
    img2sixel_color_feed(&parser, (unsigned char const *)header,
                        strlen(header), &consumed);
    if (saturate) {
        /* Reach an otherwise impractical counter boundary without huge I/O. */
        parser.payload_size = (size_t)-2;
    }
    for (i = 0u; i < 65u; ++i) {
        if (img2sixel_color_feed(&parser, block, sizeof(block), &consumed)
                != IMG2SIXEL_COLOR_PENDING || consumed != sizeof(block)) {
            fprintf(stderr, "unsupported payload stopped before ST\n");
            return EXIT_FAILURE;
        }
    }
    expected = saturate ? (size_t)-1 : 65u * sizeof(block);
    if (parser.payload_size != expected ||
        img2sixel_color_feed(&parser, (unsigned char const *)CLI_COLOR_ST,
                            2u, &consumed) != IMG2SIXEL_COLOR_UNSUPPORTED ||
        consumed != 2u || parser.definition != 2ul ||
        img2sixel_color_incomplete(&parser)) {
        fprintf(stderr, "unsupported payload lost framing or length\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int
cli_color_header_bound(void)
{
    img2sixel_color_parser_t parser;
    unsigned char header[150];
    char const *suffix;
    size_t consumed;

    suffix = "1;1;1;1;4./r" CLI_COLOR_ST;
    memset(header, '0', sizeof(header));
    header[0] = 0x1bu;
    header[1] = 'P';
    img2sixel_color_init(&parser, IMG2SIXEL_COLOR_CAPABILITY);
    if (img2sixel_color_feed(&parser, header, sizeof(header), &consumed)
            != IMG2SIXEL_COLOR_PENDING || consumed != sizeof(header) ||
        img2sixel_color_feed(&parser, (unsigned char const *)suffix,
                            strlen(suffix), &consumed)
            != IMG2SIXEL_COLOR_INVALID || consumed != strlen(suffix)) {
        fprintf(stderr, "overlong header escaped its bounded storage\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int
cli_color_consumed(void)
{
    img2sixel_color_parser_t parser;
    char const *bytes;
    size_t consumed;

    bytes = CLI_COLOR_P3 CLI_COLOR_DCS "0$r" CLI_COLOR_ST;
    img2sixel_color_init(&parser, IMG2SIXEL_COLOR_CAPABILITY);
    if (img2sixel_color_feed(&parser, (unsigned char const *)bytes,
                            strlen(bytes), &consumed)
            != IMG2SIXEL_COLOR_AVAILABLE ||
        consumed != strlen(CLI_COLOR_P3) ||
        img2sixel_color_feed(&parser, (unsigned char const *)bytes,
                            strlen(bytes), &consumed)
            != IMG2SIXEL_COLOR_AVAILABLE || consumed != 0u) {
        fprintf(stderr, "completed reply consumed a following packet\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int
test_cli_0037_color_parser(int argc, char **argv)
{
    size_t i;
    int found;

    if (argc != 2) {
        fprintf(stderr, "expected one terminal color parser case\n");
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "truncated") == 0) {
        return cli_color_truncated();
    }
    if (strcmp(argv[1], "long-payload") == 0) {
        return cli_color_long_payload(0);
    }
    if (strcmp(argv[1], "saturation") == 0) {
        return cli_color_long_payload(1);
    }
    if (strcmp(argv[1], "header-bound") == 0) {
        return cli_color_header_bound();
    }
    if (strcmp(argv[1], "consumed") == 0) {
        return cli_color_consumed();
    }
    found = 0;
    for (i = 0u; i < sizeof(cli_color_cases) / sizeof(*cli_color_cases); ++i) {
        if (strcmp(argv[1], cli_color_cases[i].group) == 0) {
            found = 1;
            if (cli_color_run_case(cli_color_cases + i) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
    }
    if (!found) {
        fprintf(stderr, "unknown terminal color parser case: %s\n", argv[1]);
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
