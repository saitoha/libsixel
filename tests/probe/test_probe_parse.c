/*
 * TAP harness covering sixel_parse_header edge cases.
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "allocator.h"
#include "probe.h"
#include "status.h"

static void
print_result(int index, int success, char const *message)
{
    printf("%s %d - %s\n", success ? "ok" : "not ok", index, message);
}

int
main(void)
{
    unsigned char const complete_header[] = { 0x1b, 'P', 'q' };
    unsigned char const truncated_header[] = { 0x1b, 'P', '"', '1', ';', '2' };
    sixel_allocator_t *allocator;
    unsigned int *params;
    size_t paramsize;
    SIXELSTATUS status;
    int case_index;
    int exit_status;

    printf("1..3\n");

    exit_status = 0;
    case_index = 1;

    status = sixel_parse_header(NULL, 0, NULL, NULL, NULL);
    if (status == SIXEL_BAD_ARGUMENT) {
        print_result(case_index, 1, "rejects null arguments");
    } else {
        print_result(case_index, 0, "unexpected acceptance of null input");
        exit_status = 1;
    }
    case_index += 1;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        print_result(case_index, 0, "allocator initialization failed");
        return 1;
    }

    params = NULL;
    paramsize = 0;
    status = sixel_parse_header(complete_header,
                                sizeof(complete_header),
                                &params,
                                &paramsize,
                                allocator);
    if (status == SIXEL_OK && params == NULL && paramsize == 0) {
        print_result(case_index, 1, "accepts minimal DCS header");
    } else {
        print_result(case_index, 0, "failed to accept minimal DCS header");
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
    if (status != SIXEL_OK && params == NULL && paramsize == 0) {
        print_result(case_index, 1, "flags incomplete header");
    } else {
        print_result(case_index, 0, "accepted incomplete header");
        exit_status = 1;
    }
    if (params != NULL) {
        sixel_allocator_free(allocator, params);
    }

    sixel_allocator_unref(allocator);

    return exit_status;
}
