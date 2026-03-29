#!/bin/sh
# TAP test confirming sixel2png accepts an unambiguous dequantize prefix.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -dk_ <"${TOP_SRCDIR}/images/map8.six" \
        >"${ARTIFACT_LOCAL_DIR}/dequantize-short.png" \
       2>"${ARTIFACT_LOCAL_DIR}/err.txt" || {
    echo "not ok" 1 - "unique dequantize prefix rejected"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/dequantize-short.png" || {
    echo "not ok" 1 - "unexpected diagnostics for -dk_"
    exit 0
}

echo "ok" 1 - "accepts unique dequantize prefix"
exit 0
