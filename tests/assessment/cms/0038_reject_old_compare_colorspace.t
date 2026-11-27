#!/bin/sh
# Reject an obsolete color-space spelling and advertise the current values.
set -eux

echo "1..1"
set -v

test -n "${LSQA_PATH-}" && test -x "${LSQA_PATH}" || {
    printf '\n%s\n' 'ok 1 # SKIP lsqa disabled'
    exit 0
}
status=0
diagnostic=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -W gamma "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1 >/dev/null) || status=$?
test "${status}" -eq 2 || {
    printf '\n%s\n' 'not ok 1 - obsolete color-space name was not rejected'
    exit 0
}
case "${diagnostic}" in
    *'compare-colorspace accepts reference, srgb, srgb-linear, oklab, lab-d65'*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - expected color-space diagnostic is missing'
    exit 0
}
printf '\n%s\n' 'ok 1 - obsolete color-space name is rejected'
exit 0
