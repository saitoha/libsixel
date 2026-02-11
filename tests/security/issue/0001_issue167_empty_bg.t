#!/bin/sh
# TAP test for issue #167 empty background argument handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

issue167="${top_srcdir}/tests/security/issue/data/167/poc"

printf '1..1\n'
set -v

run_img2sixel -B '#000' -B '' </dev/null >/dev/null && {
    fail 1 "empty background accepted unexpectedly"
    exit 0
}

test "$?" = 2 || {
    fail 1 "empty background is not rejected"
    exit 0
}

pass 1 "empty background rejected"
exit 0
