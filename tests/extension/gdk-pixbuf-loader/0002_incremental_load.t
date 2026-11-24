#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eux

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "gdk-pixbuf-loader/0002_incremental_load" >/dev/null || {
    printf '\n%s\n' "not ok 1 - gdk-pixbuf-loader/0002_incremental_load"
    exit 0
}

printf '\n%s\n' "ok 1 - gdk-pixbuf-loader/0002_incremental_load"
exit 0
