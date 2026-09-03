#!/bin/sh
# Verify PSD trace-only mode keeps its exact boolean contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_layers_minimal_type_layer.psd"
trace_only_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=1 \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    "${input_psd}" 2>/dev/null) || {
    echo "not ok" 1 - "PSD trace-only conversion failed"
    exit 0
}
invalid_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    --env SIXEL_PSD_TRACE_ONLY=on \
    --env SIXEL_PSD_TRACE_HEADER_ONLY=1 \
    "${input_psd}" 2>/dev/null) || {
    echo "not ok" 1 - "PSD invalid trace-only conversion failed"
    exit 0
}

test -z "${trace_only_output}" || {
    echo "not ok" 1 - "PSD trace-only mode emitted image output"
    exit 0
}
test -n "${invalid_output}" || {
    echo "not ok" 1 - "PSD trace-only accepted a nonnumeric boolean"
    exit 0
}

echo "ok" 1 - "PSD trace-only environment matching remains exact"
exit 0
