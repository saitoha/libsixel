#!/bin/sh
# An untagged opaque 8-bit APNG converts identically through libpng and builtin.
# Policy: docs/loader/libpng.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

input="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png"
libpng_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Llibpng! -ldisable "${input}") || {
    echo "not ok 1 - libpng APNG conversion failed"
    exit 0
}
builtin_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin! -ldisable "${input}") || {
    echo "not ok 1 - builtin APNG conversion failed"
    exit 0
}
test "${libpng_output}" = "${builtin_output}" || {
    echo "not ok 1 - libpng and builtin APNG output are identical"
    exit 0
}

echo "ok 1 - libpng and builtin APNG output are identical"
exit 0
