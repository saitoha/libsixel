#!/bin/sh
# Verify macro float output reuse with independent values and wire bytes.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" codec/macro/0016_float_output_reuse) || {
    echo "not ok 1 - macro float output reuse: native value contract"
    exit 0
}
escape=$(printf '\033')
expected="${escape}P7;0;1!z1b507122313b313b323b3123303b323b38373b33393b3337233040401b5c${escape}\\${escape}P7;0;1!z1b507122313b313b323b3123303b323b39343b33353b3335233040401b5c${escape}\\${escape}P7;0;1!z1b507122313b313b323b3123303b323b38373b33393b3337233040401b5c${escape}\\"
test "$output" = "$expected" || {
    echo "not ok 1 - macro float output reuse: exact wire contract"
    exit 0
}
echo "ok 1 - macro float output reuse"
exit 0
