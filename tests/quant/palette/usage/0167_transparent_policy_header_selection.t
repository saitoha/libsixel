#!/bin/sh
# Verify transparent-policy selects the transparent SIXEL DCS P2 value.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-static.gif"
esc="$(printf '\033')"

default_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -e -o - "${input_image}") || {
    echo "not ok" 1 - "default transparent-policy render failed"
    exit 0
}
case "${default_output}" in
    "${esc}P0;0q"*) ;;
    *)
        echo "not ok" 1 - "default transparent-policy did not emit P2=0"
        exit 0
        ;;
esac

keep_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=keep -L builtin -e -o - "${input_image}") || {
    echo "not ok" 1 - "keep transparent-policy render failed"
    exit 0
}
case "${keep_output}" in
    "${esc}P0;1q"*) ;;
    *)
        echo "not ok" 1 - "keep transparent-policy did not emit P2=1"
        exit 0
        ;;
esac

composite_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=composite -L builtin -e -o - "${input_image}") || {
    echo "not ok" 1 - "composite transparent-policy render failed"
    exit 0
}
case "${composite_output}" in
    "${esc}Pq"*) ;;
    *)
        echo "not ok" 1 - "composite transparent-policy kept explicit P2"
        exit 0
        ;;
esac

echo "ok" 1 - "transparent-policy selects background, keep, and composite headers"
exit 0
