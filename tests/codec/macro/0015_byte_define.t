#!/bin/sh
# Verify macro byte define with independent values and wire bytes.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" codec/macro/0015_byte_define) || {
    echo "not ok 1 - macro byte define: native value contract"
    exit 0
}
escape=$(printf '\033')
expected="${escape}P7;0;1!z1b507122313b313b323b3123303b323b35303b35303b3530233040401b5c${escape}\\"
test "$output" = "$expected" || {
    echo "not ok 1 - macro byte define: exact wire contract"
    exit 0
}
echo "ok 1 - macro byte define"
exit 0
