#!/bin/sh
# TAP test ensuring issue #131 PoC GIF is rejected without crashing.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



issue131="${top_srcdir}/tests/security/issue/data/131/2020-01-30-img2sixel.gif"

printf '1..1\n'
set -v

set +e
run_img2sixel --high-color "${issue131}"         >"${ARTIFACT_LOCAL_DIR}/issue131-high-color.sixel"
rc=$?
set -e

# Expected behavior:
# - The PoC must be rejected (non-zero status).
# - It must not crash (exit 139 indicates SIGSEGV).
test "${rc}" -ne 0 || {
    fail 1 "issue #131 PoC unexpectedly accepted"
    exit 0
}

test "${rc}" -ne 127 || {
    fail 1 "img2sixel was not executed as expected"
    exit 0
}

test "${rc}" -ne 139 || {
    fail 1 "issue #131 PoC triggered SIGSEGV"
    exit 0
}

pass 1 "issue #131 PoC rejected without crashing"

exit 0
