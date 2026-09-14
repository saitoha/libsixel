#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify that disabled tracing does not emit CMS apply records.
set -eux
echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC='' ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "cms/0017_builtin_byte_to_float" 2>&1) || {
    echo "not ok 1 - actual builtin float transform failed"
    exit 0
}
test -z "$trace" || {
    echo "not ok 1 - disabled CMS apply trace emitted output"
    exit 0
}
echo "ok 1 - disabled CMS apply trace is quiet"
exit 0
