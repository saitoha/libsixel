#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/emscripten.md
# Coverage: EM-03
set -eux
echo '1..1'
set -v
src_root=$1
awk -f "$src_root/tests/_static/awk/emscripten-thread-runtime.awk" \
    "$src_root/configure.ac" "$src_root/meson.build" \
    "$src_root/converters/Makefile.am" "$src_root/converters/meson.build" \
    "$src_root/assessment/Makefile.am" "$src_root/assessment/meson.build" \
    "$src_root/tests/Makefile.am" "$src_root/tests/meson.build" >&2 || {
    echo 'not ok 1 - Emscripten executable thread runtime settings'
    exit 0
}
echo 'ok 1 - Emscripten executable thread runtime settings'
exit 0
