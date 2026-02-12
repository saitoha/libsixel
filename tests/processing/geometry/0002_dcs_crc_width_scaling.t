#!/bin/sh
# Check DCS coordinates for geometry scaling combinations.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

expected_dcs_crc="302131327e2d2131327e1b5c"
digest=$(printf 'Pq"1;1;1;1!6~\'     | run_img2sixel -=1 -rne -w200%     | tr '#' '\n' | tail -n +3     | od -An -tx1 | tr -d ' \n') || digest=""

[ -n "${digest}" ] || {
    fail 1 "width scaling preserves DCS coordinates (no checksum produced)"
    exit 0
}

[ "x${digest}" = "x${expected_dcs_crc}" ] && {
    pass 1 "width scaling preserves DCS coordinates"
    exit 0
}

fail 1 "width scaling preserves DCS coordinates"

exit 0
