#!/bin/sh
# Emit TAP for specialized decoder dependency source synchronization.

set -eu

src_root=${1:-}
status=0

echo "1..1"

test -n "$src_root" || {
    echo "not ok 1 - specialized decoder sources stay synchronized"
    echo "# source root argument is required"
    exit 1
}

awk '
/^libwicsixel_la_CPPFLAGS =/ { cppflags = 1 }
/^libwicsixel_la_CFLAGS =/ { cppflags = 0 }
cppflags && /-I\$\(top_srcdir\)\/src\// { source_include = 1 }
/^if SIXEL_ENABLE_THREADS/ { conditional = 1 }
!conditional && /src\/threading\.c/ { threading = 1 }
!conditional && /src\/options-registry\.c/ { registry = 1 }
END { exit threading && registry && source_include ? 0 : 1 }
' "$src_root/wic/Makefile.am" || {
    echo "# WIC Autotools must include option sources and their private headers"
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

awk '
/^libsixelquicklookcore_a_SOURCES =/ { core = 1 }
core && /Sources\/SixelQuickLookStub\.c/ { stub = 1 }
END { exit stub ? 0 : 1 }
' "$src_root/quicklook-extension/Makefile.am" || {
    echo "# Quick Look Autotools core must include its decoder hook stub"
    status=1
}

awk '
/^srcs = files\(/ { core = 1 }
core && /Sources\/SixelQuickLookStub\.c/ { stub = 1 }
END { exit stub ? 0 : 1 }
' "$src_root/quicklook-extension/meson.build" || {
    echo "# Quick Look Meson core must include its decoder hook stub"
    status=1
}

awk '
/^sixel_test_environment_decoder_paint_thread_create_failure\(void\)/ {
    found = 1
}
END { exit found ? 0 : 1 }
' "$src_root/quicklook-extension/Sources/SixelQuickLookStub.c" || {
    echo "# Quick Look stub must provide the decoder failure hook"
    status=1
}

test "$status" -eq 0 || {
    echo "not ok 1 - specialized decoder sources stay synchronized"
    exit 1
}

echo "ok 1 - specialized decoder sources stay synchronized"
exit 0
