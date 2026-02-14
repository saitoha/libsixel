#!/bin/sh
# Check DCS coordinates for geometry scaling combinations.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

expected_dcs_payload=$(printf '%b' "0!12~-!12~\033\\")
digest=$(printf '%b' "\033Pq\"1;1;1;1!6~\033\\"     | run_img2sixel -=1 -rne -hauto -w12     | awk -F'#' '
        {
            for (idx = 3; idx <= NF; ++idx) {
                payload = payload "#" $idx
            }
        }
        END {
            sub(/^#/, "", payload)
            printf "%s", payload
        }
    ')

[ -n "${digest}" ] || {
    fail 1 "DCS coordinates stayed consistent (no payload produced)"
    exit 0
}

[ "${digest}" = "${expected_dcs_payload}" ] && {
    pass 1 "DCS coordinates stayed consistent"
    exit 0
}

fail 1 "DCS coordinates stayed consistent"

exit 0
