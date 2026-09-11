/* Verify decoder-family errors fall through but terminal failures do not. */

#include <stdio.h>

#include "src/loader-manager.h"

int
test_loader_0126_loader_manager_fallback_status(int argc, char **argv)
{
    static SIXELSTATUS const fallback_statuses[] = {
        SIXEL_FALSE,
        SIXEL_BAD_INPUT,
        SIXEL_JPEG_ERROR,
        SIXEL_PNG_ERROR,
        SIXEL_WEBP_ERROR,
        SIXEL_TIFF_ERROR,
        SIXEL_GDK_ERROR,
        SIXEL_GD_ERROR,
        SIXEL_STBI_ERROR,
        SIXEL_STBIW_ERROR,
        SIXEL_COM_ERROR,
        SIXEL_WIC_ERROR
    };
    static SIXELSTATUS const terminal_statuses[] = {
        SIXEL_OK,
        SIXEL_INTERRUPTED,
        SIXEL_BAD_ALLOCATION,
        SIXEL_BAD_ARGUMENT,
        SIXEL_BAD_INTEGER_OVERFLOW
    };
    size_t index;

    (void)argc;
    (void)argv;
    index = 0u;
    for (index = 0u;
         index < sizeof(fallback_statuses) / sizeof(fallback_statuses[0]);
         ++index) {
        if (!loader_manager_status_allows_fallback(
                fallback_statuses[index])) {
            fprintf(stderr,
                    "fallback status rejected at index %zu\n",
                    index);
            return 1;
        }
    }
    for (index = 0u;
         index < sizeof(terminal_statuses) / sizeof(terminal_statuses[0]);
         ++index) {
        if (loader_manager_status_allows_fallback(
                terminal_statuses[index])) {
            fprintf(stderr,
                    "terminal status accepted at index %zu\n",
                    index);
            return 1;
        }
    }
    return 0;
}
