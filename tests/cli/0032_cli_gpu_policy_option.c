/*
 * Test harness for --gpu-policy parsing through the public encoder option.
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include <sixel.h>

typedef struct gpu_policy_case {
    char const *value;
    int should_pass;
} gpu_policy_case_t;

int
test_cli_0032_cli_gpu_policy_option(int argc, char **argv)
{
    gpu_policy_case_t cases[] = {
        { "off", 1 },
        { "auto", 1 },
        { "force", 1 },
        { "metal", 0 },
        { NULL, 0 }
    };
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    size_t index;
    int failed;

    (void) argc;
    (void) argv;

    encoder = NULL;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status) || encoder == NULL) {
        fprintf(stderr, "failed to create encoder\n");
        return EXIT_FAILURE;
    }

    failed = 0;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_GPU_POLICY,
                                      cases[index].value);
        if (cases[index].should_pass != 0 && SIXEL_FAILED(status)) {
            fprintf(stderr, "case %zu: value should pass\n", index + 1u);
            failed = 1;
        } else if (cases[index].should_pass == 0 &&
                   SIXEL_SUCCEEDED(status)) {
            fprintf(stderr, "case %zu: value should fail\n", index + 1u);
            failed = 1;
        }
    }

    sixel_encoder_unref(encoder);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
