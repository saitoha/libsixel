#!/bin/sh
# TAP test ensuring issue #131 PoC GIF is rejected without crashing.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"

tmp_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



issue131="${top_srcdir}/tests/security/issue/data/131/2020-01-30-img2sixel.gif"
require_file "${issue131}"

printf '1..1\n'
set -v

if run_img2sixel --high-color "${issue131}" \
        >"${output_dir}/issue131-high-color.sixel" \
; then
    rc=0
else
    rc=$?
fi

# Expected behavior:
# - The PoC must be rejected (non-zero status).
# - It must not crash (exit 139 indicates SIGSEGV).
case ${rc} in
    0)
        fail 1 "issue #131 PoC unexpectedly accepted"
        ;;
    127)
        fail 1 "img2sixel was not executed as expected"
        ;;
    139)
        fail 1 "issue #131 PoC triggered SIGSEGV"
        ;;
    *)
        pass 1 "issue #131 PoC rejected without crashing"
        ;;
esac

exit "${status}"
