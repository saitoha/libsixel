#!/bin/sh
# Ensure DELTA_CHROMA emits a finite numeric value on stable fixtures.

set -eux


printf '1..1\n'
set -v

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
image_out="${TOP_SRCDIR}/tests/data/inputs/snake_64.six"
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DELTA_CHROMA "${image_ref}" "${image_out}") || {
    echo "not ok" 1 - "lsqa DELTA_CHROMA execution failed"
    exit 0
}

cr=$(printf '\r')
case "${value}" in
    *"${cr}") value=${value%"${cr}"} ;;
esac

case "${value}" in
    ""|*[!0123456789+.eE-]*)
        echo "not ok" 1 - "DELTA_CHROMA returned non-numeric output: ${value}"
        exit 0
        ;;
esac

case "${value}" in
    *[0123456789]*) ;;
    *)
        echo "not ok" 1 - "DELTA_CHROMA returned non-numeric output: ${value}"
        exit 0
        ;;
esac

test -n "${value}" || {
    echo "not ok" 1 - "DELTA_CHROMA returned non-numeric output: ${value}"
    exit 0
}

echo "ok" 1 - "DELTA_CHROMA returned a finite value"

exit 0
