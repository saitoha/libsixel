#!/bin/sh
# TAP test confirming falsey tempfile failpoints do not block stdin .svgz decode.

set -eux

test "${HAVE_LIBRSVG-}" = 1 || {
    printf "1..0 # SKIP librsvg loader is unavailable in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

svgz_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-transparent-2color.svgz"
esc="$(printf '\033')"
sixel_output=$(
    set +xv
    SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ=1
    SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_OPEN='  false  '
    SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_WRITE='  false  '
    SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_CLOSE='  false  '
    export SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ
    export SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_OPEN
    export SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_WRITE
    export SIXEL_LOADER_LIBRSVG_TEST_FAIL_TEMP_SVGZ_CLOSE
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L librsvg! - <"${svgz_path}"
) || {
    echo "not ok" 1 - "falsey tempfile failpoints unexpectedly blocked decode"
    exit 0
}

case "${sixel_output}" in
    "${esc}P0;0q"*)
        ;;
    *)
        echo "not ok" 1 - "stdin .svgz decode output header mismatch"
        exit 0
        ;;
esac

echo "ok" 1 - "falsey tempfile failpoints remain disabled"
exit 0
