#!/bin/sh
# Verify macro p3 float c1 with independent values and wire bytes.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

output=$(SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" codec/macro/0012_p3_float_c1) || {
    printf '\n%s\n' "not ok 1 - macro p3 float c1: native value contract"
    exit 0
}
escape=$(printf '\033')
expected="${escape}P7;0;1!z907122313b313b323b3123303b323b38373b33393b3337233040409c${escape}\\"
test "$output" = "$expected" || {
    printf '\n%s\n' "not ok 1 - macro p3 float c1: exact wire contract"
    exit 0
}
printf '\n%s\n' "ok 1 - macro p3 float c1"
exit 0
