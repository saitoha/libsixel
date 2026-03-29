#!/bin/sh
# Ensure HIGHFREQ_DELTA emits a finite numeric value on stable fixtures.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m HIGHFREQ_DELTA "${image_ref}" "${image_out}" | tr -d '\r') || {
    echo "not ok" 1 - "lsqa HIGHFREQ_DELTA execution failed"
    exit 0
}

case "${value}" in
    ""|*[!0123456789+.eE-]*)
        echo "not ok" 1 - "HIGHFREQ_DELTA returned non-numeric output: ${value}"
        exit 0
        ;;
esac

case "${value}" in
    *[0123456789]*) ;;
    *)
        echo "not ok" 1 - "HIGHFREQ_DELTA returned non-numeric output: ${value}"
        exit 0
        ;;
esac

test -n "${value}" || {
    echo "not ok" 1 - "HIGHFREQ_DELTA returned non-numeric output: ${value}"
    exit 0
}

echo "ok" 1 - "HIGHFREQ_DELTA returned a finite value"

exit 0
