#!/bin/sh
# Verify macro float animation replay with independent values and wire bytes.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" codec/macro/0014_float_animation_replay) || {
    echo "not ok 1 - macro float animation replay: native value contract"
    exit 0
}
escape=$(printf '\033')
expected="${escape}[H${escape}P0;0;1!z1b507122313b313b323b3123303b323b33353b33353b3335233040401b5c${escape}\\${escape}[0*z${escape}[H${escape}P1;0;1!z1b507122313b313b323b3123303b323b33373b33373b3337233040401b5c${escape}\\${escape}[1*z${escape}[H${escape}[0*z${escape}[H${escape}[1*z"
test "$output" = "$expected" || {
    echo "not ok 1 - macro float animation replay: exact wire contract"
    exit 0
}
echo "ok 1 - macro float animation replay"
exit 0
