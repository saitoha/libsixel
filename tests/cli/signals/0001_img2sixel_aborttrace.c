/*
 * Abort trace test that triggers SIGABRT from the test runner.
 *
 * The wrapper script checks stderr for the abort trace markers, so this
 * helper only needs to install the handler and abort. This avoids external
 * signal injection while still exercising the same signal path.
 *
 * Flow:
 *   - install abort handler
 *   - call abort()
 *   - let the wrapper validate the emitted markers
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "converters/aborttrace.h"

int
test_aborttrace_0001_img2sixel_aborttrace(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if !defined(SIXEL_ENABLE_ABORT_TRACE)
    fprintf(stderr, "abort trace disabled at build time\n");
    return 77;
#else
    fprintf(stderr, "aborttrace: runner triggered\n");
    sixel_aborttrace_install_if_unhandled();
    /*
     * Policy tests can inspect the installation decision without triggering
     * the platform's unhandled-abort path. Headless Haiku may suspend there
     * while waiting for its debugger service.
     */
    if (argc > 1 && strcmp(argv[1], "install-only") == 0) {
        return EXIT_SUCCESS;
    }
    abort();
#if defined(__TINYC__)
    /*
     * tcc does not treat abort() as noreturn here. Keep this fallback hidden
     * from MSVC, where the same statement is reported as unreachable code.
     */
    return EXIT_FAILURE;
#endif
#endif
}
