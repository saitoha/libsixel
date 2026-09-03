#!/bin/sh
# Verify sixel2png applies abort tracing after diagnostics option parsing.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

trace_message=$(set +xv; ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" \
    --env SIXEL_TRACE_TOPIC=suboption_contract,aborttrace_contract \
    --env SIXEL_ABORT_TRACE=1 -xhuman:A0 \
    -i "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    -o /dev/null 2>&1) || {
    echo "not ok" 1 - "sixel2png abort trace conversion failed"
    exit 0
}

test "${trace_message#*LSXSUB1|*key=abort_trace|stored=1|binding=abort_trace,abort_trace_override|value=0*}" != "${trace_message}" || {
    echo "not ok" 1 - "sixel2png abort trace value was not stored"
    exit 0
}
test "${trace_message#*LSXABT1|*enabled=0|installed=0*}" != \
    "${trace_message}" || {
    echo "not ok" 1 - "sixel2png abort trace value missed its consumer"
    exit 0
}

echo "ok" 1 - "sixel2png applies abort trace after option parsing"
exit 0
