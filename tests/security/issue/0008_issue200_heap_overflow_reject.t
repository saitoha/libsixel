#!/bin/sh
# TAP test for issue #200 regression using the reported CLI flags.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



check_exit() {
    if run_img2sixel "$@"; then
        rc=0
    else
        rc=$?
    fi

    # Accept success or mapped error exits (1/2/3) without crashing.
    case ${rc} in
        0|1|2|3)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

issue200="${top_srcdir}/tests/security/issue/data/200/POC_img2sixel_heap_buffer_overflow"

printf '1..1\n'
set -v

if check_exit --7bit-mode -8 --invert --palette-type=auto --verbose \
        "${issue200}" -o /dev/null; then
    pass 1 "reported heap overflow path rejected safely"
else
    fail 1 "reported heap overflow path failed"
fi

exit "${status}"
