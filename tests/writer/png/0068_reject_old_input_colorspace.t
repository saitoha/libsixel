#!/bin/sh
# Reject an obsolete color-space spelling and advertise the current values.
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP sixel2png disabled'
    exit 0
}
status=0
diagnostic=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    -T adobe-rgb </dev/null 2>&1 >/dev/null) || status=$?
test "${status}" -eq 2 || {
    printf '\n%s\n' 'not ok 1 - obsolete color-space name was not rejected'
    exit 0
}
case "${diagnostic}" in
    *'unknown option base value "adobe-rgb". valid values: srgb, display-p3, a98-rgb'*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - expected color-space diagnostic is missing'
    exit 0
}
printf '\n%s\n' 'ok 1 - obsolete color-space name is rejected'
exit 0
