#!/bin/sh
# TAP test confirming --loaders wic,builtin falls back for GIF input.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

test "${HAVE_WIC-}" = 1 || {
    skip_all "wic loader is unavailable"
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"

run_img2sixel -L wic,builtin "${input_gif}" >/dev/null || {
    fail 1 "wic,builtin fallback GIF decoding failed"
    exit 0
}

pass 1 "wic,builtin fallback GIF decoding succeeds"
exit 0
