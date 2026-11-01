#!/bin/sh
# Rejected macro delta emits nothing; full recovery retains its wire contract.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" palette/delta_native/0230_delta_macro_byte_recovery) || {
    echo "not ok 1 - macro byte recovery state"
    exit 0
}
escape=$(printf '\033')
# One opaque white pixel, a two-entry black/white palette, and no delta body.
expected="${escape}P0;0;1!z1b507122313b313b313b3123303b323b303b303b3023313b323b3130303b3130303b3130302331401b5c${escape}\\${escape}[0*z"
test "$output" = "$expected" || {
    echo "not ok 1 - macro byte recovery wire"
    exit 0
}
echo "ok 1 - macro byte recovery"
exit 0
