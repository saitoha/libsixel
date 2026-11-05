#!/bin/sh
# Dependency metadata must not enable an unusable JPEG loader.
# Policy: docs/loader/libjpeg.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
awk '
function valid(configure, meson) {
    return configure ~ /AC_LINK_IFELSE/ &&
        configure ~ /#include <jpeglib.h>/ &&
        configure ~ /jpeg_std_error\(&error\)/ &&
        configure ~ /have_jpeg=no/ &&
        configure ~ /LIBJPEG_CFLAGS=[[:space:]]+LIBJPEG_LIBS=/ &&
        meson ~ /if not cc.links/ && meson ~ /#include <jpeglib.h>/ &&
        meson ~ /jpeg_std_error\(&error\)/ &&
        meson ~ /dependencies: jpeg_dep/ &&
        meson ~ /jpeg_opt.enabled\(\)/ && meson ~ /error\(/ &&
        meson ~ /jpeg_dep = disabler\(\)/
}
/AC_MSG_CHECKING\(\[whether libjpeg headers and library are usable/ { c = 1 }
c { configure = configure $0 "\n" }
c && /LIBS=\$JPEG_CHECK_LIBS_BACKUP/ { c = 0 }
/if not cc.links/ { candidate = $0 "\n"; m = 1; next }
m { candidate = candidate $0 "\n" }
m && /jpeg_dep = disabler/ { meson = candidate; m = 0 }
END {
    compile = configure
    sub(/AC_LINK_IFELSE/, "AC_COMPILE_IFELSE", compile)
    header = configure
    sub(/#include <jpeglib.h>/, "", header)
    enabled = configure
    sub(/have_jpeg=no/, "have_jpeg=yes", enabled)
    unused = meson
    sub(/dependencies: jpeg_dep/, "", unused)
    missing = meson
    sub(/jpeg_dep = disabler\(\)/, "", missing)
    exit !valid(configure, meson) || valid(compile, meson) ||
        valid(header, meson) || valid(enabled, meson) ||
        valid(configure, unused) || valid(configure, missing)
}
' "$1/configure.ac" "$1/meson.build" || {
    echo 'not ok 1 - libjpeg activation requires its header and linked API'
    exit 0
}
echo 'ok 1 - libjpeg activation requires its header and linked API'
exit 0
