#!/bin/sh
# Emit TAP for standalone WIC option dependency source synchronization.

set -eu

src_root=${1:-}
status=0

echo "1..1"

test -n "$src_root" || {
    echo "not ok 1 - WIC option dependency sources stay synchronized"
    echo "# source root argument is required"
    exit 1
}

awk '
/^if SIXEL_ENABLE_THREADS/ { conditional = 1 }
!conditional && /src\/threading\.c/ { threading = 1 }
!conditional && /src\/options-registry\.c/ { registry = 1 }
END { exit threading && registry ? 0 : 1 }
' "$src_root/wic/Makefile.am" || {
    echo "# wic/Makefile.am must include threading.c and options-registry.c unconditionally"
    status=1
}

awk '
/^if conf\.get\(.SIXEL_ENABLE_THREADS./ { conditional = 1 }
!conditional && /src\/threading\.c/ { threading = 1 }
!conditional && /src\/options-registry\.c/ { registry = 1 }
END { exit threading && registry ? 0 : 1 }
' "$src_root/wic/meson.build" || {
    echo "# wic/meson.build must include threading.c and options-registry.c unconditionally"
    status=1
}

awk '
/^sixel_test_environment_decoder_paint_thread_create_failure\(void\)/ {
    found = 1
}
END { exit found ? 0 : 1 }
' "$src_root/wic/wic_stub.c" || {
    echo "# wic/wic_stub.c must provide the decoder failure-injection broker"
    status=1
}

test "$status" -eq 0 || {
    echo "not ok 1 - WIC option dependency sources stay synchronized"
    exit 1
}

echo "ok 1 - WIC option dependency sources stay synchronized"
exit 0
