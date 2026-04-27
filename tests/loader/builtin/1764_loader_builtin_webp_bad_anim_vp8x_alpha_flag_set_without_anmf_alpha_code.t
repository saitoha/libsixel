#!/bin/sh
# TAP test confirming VP8X alpha=1 without ANMF VP8+ALPH is rejected.
# Fixture is derived from animated-lossy-8x8-2frame-min.webp.
# Patch: VP8X flag byte at offset 0x14 changed from 0x02 to 0x12.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_webp="${TOP_SRCDIR}/tests/data/corrupted/bad_anim_vp8x_alpha_flag_set_without_anmf_alpha.webp"
trace_output=''
diag_line=''
command_status=0
nl='
'

SIXEL_TRACE_TOPIC=webp_decode
export SIXEL_TRACE_TOPIC
trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin! -o /dev/null "${input_webp}" 2>&1) || command_status=$?

test "${command_status}" -ne 0 || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8x_alpha_flag_set_without_anmf_alpha.webp unexpectedly succeeded"
    exit 0
}

diag_line=${trace_output#*LSXWEBP1\|}
test "${diag_line}" != "${trace_output}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8x_alpha_flag_set_without_anmf_alpha.webp missing LSXWEBP1 contract header"
    exit 0
}

diag_line="LSXWEBP1|${diag_line}"
diag_line=${diag_line%%"${nl}"*}

test "${diag_line#LSXWEBP1\|rc=1\|kind=ERR\|codes=}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8x_alpha_flag_set_without_anmf_alpha.webp malformed error contract header"
    exit 0
}

test "${diag_line#*W_ERR_VP8X_FLAG_ALPHA_MISMATCH*}" != "${diag_line}" || {
    echo "not ok" 1 - "forced builtin loader bad_anim_vp8x_alpha_flag_set_without_anmf_alpha.webp missing W_ERR_VP8X_FLAG_ALPHA_MISMATCH"
    exit 0
}

echo "ok" 1 - "forced builtin loader bad_anim_vp8x_alpha_flag_set_without_anmf_alpha.webp emits W_ERR_VP8X_FLAG_ALPHA_MISMATCH"
exit 0
