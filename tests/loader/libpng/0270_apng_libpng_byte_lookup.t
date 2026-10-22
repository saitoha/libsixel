#!/bin/sh
# An untagged opaque 8-bit libpng APNG keeps byte palette lookup.
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

# No color metadata, no fractional coverage and an 8-bit source keep every
# canvas value a decoded byte, so palette apply must stay on byte lookup.
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibpng! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - libpng APNG conversion failed"
    exit 0
}
case "${trace}" in
    *selected=lookup/*.float32*) false ;;
    *selected=lookup/*.8bit*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - libpng APNG frames select byte lookup"
    exit 0
}

echo "ok 1 - libpng APNG frames select byte lookup"
exit 0
