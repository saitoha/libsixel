#!/bin/sh
# Verify macro p3 float mask with independent values and wire bytes.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" codec/macro/0013_p3_float_mask) || {
    echo "not ok 1 - macro p3 float mask: native value contract"
    exit 0
}
escape=$(printf '\033')
# Default clear requests P2=0. The omission-only palette slot is not
# serialized; two data bits paint the visible pixels, never the third.
expected="${escape}P7;0;1!z1b50303b307122313b313b333b3123303b323b38373b33393b3337233040401b5c${escape}\\"
test "$output" = "$expected" || {
    echo "not ok 1 - macro p3 float mask: exact wire contract"
    exit 0
}
echo "ok 1 - macro p3 float mask"
exit 0
