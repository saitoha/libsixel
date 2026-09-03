#!/bin/sh
# Verify PSD header-only mode keeps its exact boolean contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_layers_minimal_type_layer.psd"
header_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    "${input_psd}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "PSD header-only conversion failed"
    exit 0
}
invalid_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=on \
    "${input_psd}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "PSD invalid header-only conversion failed"
    exit 0
}

test "${header_trace#*LSXPSD1|*}" != "${header_trace}" || {
    echo "not ok" 1 - "PSD header-only mode omitted its header"
    exit 0
}
test "${header_trace#*libsixel?psd_decode?:*}" = "${header_trace}" || {
    echo "not ok" 1 - "PSD header-only mode retained verbose traces"
    exit 0
}
test "${invalid_trace#*libsixel?psd_decode?:*}" != "${invalid_trace}" || {
    echo "not ok" 1 - "PSD header-only accepted a nonnumeric boolean"
    exit 0
}

echo "ok" 1 - "PSD header-only environment matching remains exact"
exit 0
