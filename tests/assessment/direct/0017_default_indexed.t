#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Omitting -D retains auto indexed evaluation.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_E00 "${TOP_SRCDIR}/tests/data/color-numeric/stable.six" "${TOP_SRCDIR}/tests/data/color-numeric/redefined.six") || {
    printf '\n%s\n' "not ok 1 - evaluation failed"
    exit 0
}
# The archived pre-direct CLI produced this six-decimal indexed result.
test "${value%%[!0-9.]*}" = 42.156315 || {
    printf '\n%s\n' "not ok 1 - indexed default changed: $value"
    exit 0
}
printf '\n%s\n' "ok 1 - default indexed"
exit 0
