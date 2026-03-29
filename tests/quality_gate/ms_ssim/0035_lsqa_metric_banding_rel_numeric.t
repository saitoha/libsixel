#!/bin/sh
# Ensure BANDING_REL emits a finite numeric value on stable fixtures.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m BANDING_REL "${image_ref}" "${image_out}" | tr -d '\r') || {
    echo "not ok" 1 - "lsqa BANDING_REL execution failed"
    exit 0
}

case "${value}" in
    ""|*[!0123456789+.eE-]*)
        echo "not ok" 1 - "BANDING_REL returned non-numeric output: ${value}"
        exit 0
        ;;
esac

case "${value}" in
    *[0123456789]*) ;;
    *)
        echo "not ok" 1 - "BANDING_REL returned non-numeric output: ${value}"
        exit 0
        ;;
esac

test -n "${value}" || {
    echo "not ok" 1 - "BANDING_REL returned non-numeric output: ${value}"
    exit 0
}

echo "ok" 1 - "BANDING_REL returned a finite value"

exit 0
