#!/bin/sh
# Verify handoff trace minimization keeps its exact boolean contract.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
minimal_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=encode_handoff \
    --env SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=1 \
    "${input_image}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "minimal handoff trace conversion failed"
    exit 0
}
invalid_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=encode_handoff \
    --env SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=on \
    "${input_image}" -o/dev/null 2>&1) || {
    echo "not ok" 1 - "invalid handoff trace conversion failed"
    exit 0
}

test "${minimal_trace#*event=callback_handoff_decide*}" != \
    "${minimal_trace}" || {
    echo "not ok" 1 - "minimal handoff trace omitted the decision event"
    exit 0
}
test "${minimal_trace#*event=callback_enter*}" = "${minimal_trace}" || {
    echo "not ok" 1 - "minimal handoff trace retained verbose events"
    exit 0
}
test "${invalid_trace#*event=callback_enter*}" != "${invalid_trace}" || {
    echo "not ok" 1 - "handoff trace accepted a nonnumeric boolean"
    exit 0
}

echo "ok" 1 - "handoff trace environment matching remains exact"
exit 0
