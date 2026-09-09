#!/bin/sh
# Policy: docs/functionality/cover-policy.md
# Verify cover policy conflicts with a built-in palette.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"

set +e
msg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
          --cover-policy=off -bxterm256 \
          -o/dev/null "${input_image}" 2>&1 >/dev/null)
command_status=$?
set -e

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "unexpected success: cover policy conflicts with built-in palette"
    exit 0
}
test "${msg#*option -b, --builtin-palette conflicts with -a, --cover-policy.*}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing cover policy and built-in conflict diagnostic"
    exit 0
}

echo "ok" 1 - "cover policy then built-in palette is rejected"
exit 0
