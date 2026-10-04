#!/bin/sh
# Verify macro p3 float invoke with independent values and wire bytes.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" codec/macro/0011_p3_float_invoke) || {
    echo "not ok 1 - macro p3 float invoke: native value contract"
    exit 0
}
escape=$(printf '\033')
expected="${escape}P0;0;1!z1b507122313b313b323b3123303b323b38373b33393b3337233040401b5c${escape}\\${escape}[0*z"
test "$output" = "$expected" || {
    echo "not ok 1 - macro p3 float invoke: exact wire contract"
    exit 0
}
echo "ok 1 - macro p3 float invoke"
exit 0
