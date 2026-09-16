#!/bin/sh
# Policy: docs/loader/color-management.md
# independent matched values and prewrite profile-domain rejection
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0313_lcms2_rgb_float_profile_format_gamma 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required CMS or loader unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - independent matched values and prewrite profile-domain rejection"
    exit 0
}
echo "ok 1 - 0313_lcms2_rgb_float_profile_format_gamma independent matched values and prewrite profile-domain rejection"
exit 0
