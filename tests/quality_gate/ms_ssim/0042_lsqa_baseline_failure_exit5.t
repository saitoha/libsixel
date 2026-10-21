#!/bin/sh
# Verify that lsqa exits with code 5 when baseline is above measured value.

# The five-scale metric requires at least 176 pixels in both dimensions.
# The small non-identity PNG/SIXEL pair retains parser and exit-status coverage.
set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_192.png"
image_out="${TOP_SRCDIR}/tests/data/inputs/progressive-snake-192-baseline.six"
metric=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "failed to calculate MS-SSIM"
    exit 0
}
case "${metric}" in
    1.*)
        baseline="1.100000"
        ;;
    *)
        baseline="1.000000"
        ;;
esac

test -n "${metric}" || {
    echo "not ok" 1 - "failed to calculate MS-SSIM"
    exit 0
}

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${baseline}" \
    "${image_ref}" "${image_out}" >/dev/null 2>&1
status=$?
set -e

test "${status}" -eq 5 || {
    echo "not ok" 1 - "expected exit code 5, got ${status}"
    exit 0
}

echo "ok" 1 - "baseline above metric returned exit code 5"

exit 0
