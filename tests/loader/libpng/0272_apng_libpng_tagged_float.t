#!/bin/sh
# A gAMA-tagged libpng APNG keeps float lookup even with CMS disabled.
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

# The same frames as apng_8x8_rgb_loop2.png tagged with gAMA are not sRGB
# bytes; disabling CMS must not reinterpret them as such.
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng! -ldisable \
    --cms-engine=none \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2_gama.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - tagged libpng APNG conversion failed"
    exit 0
}
case "${trace}" in
    *selected=lookup/*.8bit*) false ;;
    *selected=lookup/*.float32*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - color-tagged APNG keeps float lookup without CMS"
    exit 0
}

echo "ok 1 - color-tagged APNG keeps float lookup without CMS"
exit 0
