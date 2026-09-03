#!/bin/sh
# Verify sixel2png applies the shared trace topic suboption.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace_status=0
trace_message=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_TRACE_TOPIC=suboption_contract \
    -xhuman:Tsuboption_contract -d k_undithr 2>&1) || trace_status=$?

test "${trace_status}" -eq 255 || {
    echo "not ok" 1 - "trace topic diagnostics exit status mismatch"
    exit 0
}

test "${trace_message#*LSXSUB1|*key=trace_topic|stored=1|binding=trace_topic,trace_topic_override|value=suboption_contract*}" != "${trace_message}" || {
    echo "not ok" 1 - "trace topic was not applied by sixel2png"
    exit 0
}

echo "ok" 1 - "sixel2png applies the shared trace topic suboption"
exit 0
