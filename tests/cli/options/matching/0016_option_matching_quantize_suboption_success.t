#!/bin/sh
# TAP test verifying -Q accepts kmeans suboptions with short prefixes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="quantize_suboption_success"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"

: >"${out_file}"
: >"${err_file}"

run_img2sixel -Qk:i=p:t=0.120 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" || {
    fail 1 "-Q kmeans suboptions were rejected"
    exit 0
}

pass 1 "-Q accepts kmeans suboptions"
exit 0
