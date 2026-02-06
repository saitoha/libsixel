#!/bin/sh
# Validate DCS coordinates when height scaled and width auto.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

check_dcs_crc() {
    case_no=$1
    scale_args=$2
    description=$3
    expected_dcs_crc="302131327e2d2131327e1b5c"

    digest=$(printf '\033Pq"1;1;1;1!6~\033\\' \
        | run_img2sixel -rne ${scale_args} \
        | tr '#' '\n' | tail -n +3 \
        | od -An -tx1 | tr -d ' \n') || digest=""

    if [ -z "${digest}" ]; then
        fail "${case_no}" "${description} (no checksum produced)"
        return
    fi

    if [ "x${digest}" = "x${expected_dcs_crc}" ]; then
        pass "${case_no}" "${description}"
    else
        fail "${case_no}" "${description}"
    fi
}
check_dcs_crc 1 "-h200% -wauto" \
    "automatic width with height scaling stays consistent"

exit "${status}"
