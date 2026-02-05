#!/bin/sh
# TAP test for issue #51 stbi_1561_poc.bin regression.
# Ensure the converter rejects the input without crashing, even under ASan.

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

issue51="${TOP_SRCDIR}/tests/security/issue/data/libsixel-libsixel/51/stbi_1561_poc.bin"

printf '1..1\n'
set -v

# Use the minimal invocation to exercise the decoder and ensure the
# reported PoC is rejected safely.
if check_exit "${issue51}" -o /dev/null; then
    pass 1 "issue #51 PoC rejected safely"
else
    fail 1 "issue #51 PoC handling failed"
fi

exit "${status}"
