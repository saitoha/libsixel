#!/bin/sh
# Policy: docs/functionality/cover-policy.md
# Verify monochrome conflicts with cover policy.
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
          -e --cover-policy=off -o/dev/null "${input_image}" \
          2>&1 >/dev/null)
command_status=$?
set -e

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "unexpected success: monochrome conflicts with cover policy"
    exit 0
}
test "${msg#*option -a, --cover-policy conflicts with -e, --monochrome.*}" \
    != "${msg}" || {
    echo "not ok" 1 - "missing monochrome and cover policy conflict diagnostic"
    exit 0
}

echo "ok" 1 - "monochrome then cover policy is rejected"
exit 0
