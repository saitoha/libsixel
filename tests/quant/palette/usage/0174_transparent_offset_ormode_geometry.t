#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Verify transparent-offset places OR-mode bodies on the offset geometry.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -b xterm16 -d none --palette-type=rgb -O -+ 3,5 -o - <<'PPM'
P3
2 2
255
255 0 0   0 255 0
0 0 255   255 255 255
PPM
) || {
    echo "not ok 1 - OR offset encode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/or_offset_cli" "${output}" || {
    echo "not ok 1 - OR offset pixels or geometry differ"
    exit 0
}

echo "ok 1 - OR offset preserves every source and margin pixel"
exit 0
