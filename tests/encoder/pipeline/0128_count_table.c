/* SPDX-License-Identifier: MIT */
/* Exhaust every table entry against independent snprintf,
 * with unaligned destinations and guards around the five-byte reservation. */
#include <stdio.h>
#include <string.h>
#include "src/encoder-count.h"

int
test_encode_count_0128(int argc, char **argv)
{
    static int const tails[] = {-1, 0, '?', '~', 255};
    unsigned char buffer[21];
    unsigned char *end;
    char expected[16];
    int value;
    int tail;
    int index;
    int offset;
    int length;
    int i;

    (void)argc;
    (void)argv;
    for (value = 0; value < 1000; value++) {
        for (index = 0; index < 5; index++) {
            tail = tails[index];
            length = snprintf(expected, sizeof(expected), "%c%d",
                tail < 0 ? '#' : '!', value);
            if (tail >= 0) expected[length++] = (char)tail;
            for (offset = 1; offset <= 8; offset++) {
                memset(buffer, 0xa5, sizeof(buffer));
                end = sixel_put_count_reserved(buffer + offset,
                    tail < 0 ? '#' : '!', value, tail);
                if (end != buffer + offset + length ||
                        memcmp(buffer + offset, expected, (size_t)length)) {
                    fprintf(stderr, "count=%d tail=%d offset=%d\n",
                        value, tail, offset);
                    return 1;
                }
                for (i = 0; i < (int)sizeof(buffer); i++) {
                    if ((i < offset || i >= offset + 5) &&
                            buffer[i] != 0xa5) return 1;
                }
            }
        }
    }
    return 0;
}
