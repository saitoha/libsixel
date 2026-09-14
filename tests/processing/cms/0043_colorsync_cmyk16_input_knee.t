#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify original integer input and actual colorsync cmyk16 input knee conversion.
set -eux
echo "1..1"
set -v

status=0
SIXEL_LOADER_CMS_RENDERING_INTENT=relative! ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0033_integer_cmyk_float" colorsync 16 separate knee || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - colorsync cmyk16 input knee # SKIP colorsync unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - colorsync cmyk16 input knee"
    exit 0
}
echo "ok 1 - colorsync cmyk16 input knee"
exit 0
