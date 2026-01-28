#!/bin/sh
# TAP test for issue #73 stbi_1561_poc.bin regression.
# Ensure the converter rejects the input without crashing, even under ASan.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/regression.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



check_exit() {
    if run_img2sixel "$@" 2>>"${log_file}"; then
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

issue73="${top_srcdir}/tests/issue/libsixel-libsixel/73/stbi_1561_poc.bin"
require_file "${issue73}"

printf '1..1\n'
set -v

# Use the minimal invocation to exercise the decoder and ensure the
# reported PoC is rejected safely.
if check_exit "${issue73}" -o /dev/null; then
    pass 1 "issue #73 PoC rejected safely"
else
    fail 1 "issue #73 PoC handling failed"
fi

exit "${status}"
