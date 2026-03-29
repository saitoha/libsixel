#!/bin/sh
# TAP test confirming falsey relative-resource opt-in keeps default blocking.

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

svg_path="${TOP_SRCDIR}/tests/data/inputs/formats/librsvg-relative-image.svg"
esc="$(printf '\033')"
sixel_output=$(
    set +xv
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES=false \
        -L librsvg! "${svg_path}"
) || {
    echo "not ok" 1 - "relative-resource falsey env conversion failed"
    exit 0
}

case "${sixel_output}" in
    "${esc}P0;1q"*)
        ;;
    *)
        echo "not ok" 1 - "falsey opt-in unexpectedly enabled relative resource"
        exit 0
        ;;
esac

echo "ok" 1 - "falsey relative-resource opt-in keeps blocking behavior"
exit 0
