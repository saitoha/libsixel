/*
 * Harness covering sixel_parse_header edge cases.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "allocator.h"
#include "probe.h"
#include "status.h"

int
test_probe_0001_probe_parse(int argc, char **argv)
{
    unsigned char const complete_header[] = { 0x1b, 'P', 'q' };
    unsigned char const truncated_header[] = { 0x1b, 'P', '"', '1', ';', '2' };
    sixel_allocator_t *allocator;
    unsigned int *params;
    size_t paramsize;
    SIXELSTATUS status;
    int case_index;
    int exit_status;

    (void) argc;
    (void) argv;

    exit_status = 0;
    case_index = 1;

    status = sixel_parse_header(NULL, 0, NULL, NULL, NULL);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "case %d: unexpected acceptance of null input\n",
                case_index);
        exit_status = 1;
    }
    case_index += 1;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "case %d: allocator initialization failed\n",
                case_index);
        return EXIT_FAILURE;
    }

    params = NULL;
    paramsize = 0;
    status = sixel_parse_header(complete_header,
                                sizeof(complete_header),
                                &params,
                                &paramsize,
                                allocator);
    if (status != SIXEL_OK || params != NULL || paramsize != 0) {
        fprintf(stderr, "case %d: failed to accept minimal DCS header\n",
                case_index);
        exit_status = 1;
    }
    if (params != NULL) {
        sixel_allocator_free(allocator, params);
    }
    case_index += 1;

    params = NULL;
    paramsize = 0;
    status = sixel_parse_header(truncated_header,
                                sizeof(truncated_header),
                                &params,
                                &paramsize,
                                allocator);
    if (status == SIXEL_OK || params != NULL || paramsize != 0) {
        fprintf(stderr, "case %d: accepted incomplete header\n", case_index);
        exit_status = 1;
    }
    if (params != NULL) {
        sixel_allocator_free(allocator, params);
    }

    sixel_allocator_unref(allocator);

    return exit_status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
