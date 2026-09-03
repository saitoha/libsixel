#!/bin/sh
# Verify trace topics retain token separators and exact matching.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
matched_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env 'SIXEL_TRACE_TOPIC=unused;lifecycle' \
    "${input_image}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "separated trace topic conversion failed"
    exit 0
}
partial_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=lifecycl \
    "${input_image}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "partial trace topic conversion failed"
    exit 0
}

test "${matched_trace#*img2sixel?lifecycle?:*}" != \
    "${matched_trace}" || {
    echo "not ok" 1 - "semicolon-separated trace topic was not matched"
    exit 0
}
test -z "${partial_trace}" || {
    echo "not ok" 1 - "trace topic accepted a partial token"
    exit 0
}

echo "ok" 1 - "trace topic token matching remains exact"
exit 0
