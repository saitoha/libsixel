/*
 * SPDX-License-Identifier: MIT
 *
 * Verify registry-backed dense packing for direct lookup component users.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/compat_stub.h"
#include "src/lookup-policy.h"

int
test_lookup_0012_packing_registry_direct(int argc, char **argv)
{
    sixel_lookup_policy_prepare_request_t request;
    int actual;

    (void)argc;
    (void)argv;
    memset(&request, 0, sizeof(request));
    actual = SIXEL_LOOKUP_PACK_LINEAR;

    if (sixel_compat_setenv("SIXEL_LOOKUP_PACKING", "MoRtOn") != 0) {
        fprintf(stderr, "failed to set dense packing environment\n");
        return EXIT_FAILURE;
    }
    actual = sixel_lookup_policy_resolve_packing(&request, "5bit");
    if (actual != SIXEL_LOOKUP_PACK_MORTON) {
        fprintf(stderr, "5bit direct environment packing was not applied\n");
        return EXIT_FAILURE;
    }
    actual = sixel_lookup_policy_resolve_packing(&request, "6bit");
    if (actual != SIXEL_LOOKUP_PACK_MORTON) {
        fprintf(stderr, "6bit direct environment packing was not applied\n");
        return EXIT_FAILURE;
    }
    actual = sixel_lookup_policy_resolve_packing(&request, "fhedt");
    if (actual != SIXEL_LOOKUP_PACK_LINEAR) {
        fprintf(stderr, "dense packing escaped its declared lookup bases\n");
        return EXIT_FAILURE;
    }

    request.lut_policy_packing = SIXEL_LOOKUP_PACK_HILBERT;
    request.lut_policy_packing_override = 1;
    if (sixel_compat_setenv("SIXEL_LOOKUP_PACKING", "linear") != 0) {
        fprintf(stderr, "failed to replace dense packing environment\n");
        return EXIT_FAILURE;
    }
    actual = sixel_lookup_policy_resolve_packing(&request, "5bit");
    if (actual != SIXEL_LOOKUP_PACK_HILBERT) {
        fprintf(stderr, "direct packing override lost precedence\n");
        return EXIT_FAILURE;
    }

    (void)sixel_compat_setenv("SIXEL_LOOKUP_PACKING", "");
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
