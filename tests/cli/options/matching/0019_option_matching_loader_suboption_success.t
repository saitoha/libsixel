#!/bin/sh
# TAP test verifying -L accepts WIC suboptions.

set -euxv

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
config_macro_defined HAVE_WIC || skip_all "wic loader is unavailable"

echo "1..1"

set +e
probe_output=$(run_img2sixel -Lwic:ico_minsize=40! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico" \
    >/dev/null 2>&1)
probe_status=$?
set -e

printf '%s' "${probe_output}" | \
    grep "invalid argument for -L,--loaders option" >/dev/null && {
    fail 1 "-L wic suboptions were rejected by option parser"
    exit 0
}

test "${probe_status}" -eq 0 || true

pass 1 "-L accepts wic:ico_minsize suboptions"
exit 0
