#!/bin/sh
# Reject an obsolete output color-space spellings.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '1..0 # SKIP img2sixel is disabled\n'
    exit 0
}

printf '1..1\n'
set -v

status=0
diagnostic=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -U "linear" "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" \
    2>&1 >/dev/null) || status=$?
test "$status" -eq 2 || {
    printf '\n%s\n' "not ok 1 - obsolete output colorspace linear accepted"
    exit 0
}
case "$diagnostic" in
    *"colorspace"*|*"option base value"*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' "not ok 1 - obsolete output colorspace diagnostic missing"
    exit 0
}

printf '\n%s\n' 'ok 1 - obsolete output colorspace is rejected'
exit 0
