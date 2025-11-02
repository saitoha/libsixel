#!/usr/bin/env bash
# Verify handling of shifted option arguments.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

shift_err="${TMP_DIR}/argument-shift.err"
shift_out="${TMP_DIR}/argument-shift.out"
json_out="${TMP_DIR}/argument-shift-json.out"

abs_top=""
if ! abs_top=$(cd "${TOP_SRCDIR}" && pwd); then
    echo 'failed to resolve image path' >&2
    exit 1
fi
image_path="${abs_top}/images/snake.jpg"

rm -f "${shift_err}" "${shift_out}" "${json_out}"

echo '[test1] detect missing mapfile argument despite option-shaped token'

if run_img2sixel -m -w 100 -h 100 "${image_path}" \
        >"${shift_out}" 2>"${shift_err}"; then
    echo 'img2sixel unexpectedly accepted -m without an argument' >&2
    rm -f "${shift_err}" "${shift_out}"
    exit 1
fi
if ! grep -q 'missing required argument for -m,--mapfile option' \
        "${shift_err}"; then
    echo 'img2sixel did not report a missing mapfile argument' >&2
    cat "${shift_err}" >&2 || :
    rm -f "${shift_err}" "${shift_out}"
    exit 1
fi

pushd "${TMP_DIR}" >/dev/null

echo '[test2] allow outfile values that look like options'

rm -f -- -p
if ! run_img2sixel -o -p "${image_path}" \
        >/dev/null 2>"${shift_err}"; then
    echo 'img2sixel rejected output file named -p' >&2
    cat "${shift_err}" >&2 || :
    popd >/dev/null
    rm -f "${shift_err}" "${shift_out}" "${json_out}"
    exit 1
fi
if [[ ! -s -p ]]; then
    echo 'img2sixel did not create output file named -p' >&2
    popd >/dev/null
    rm -f "${shift_err}" "${shift_out}" "${json_out}"
    exit 1
fi
rm -f -- -p

echo '[test3] allow assessment file values that look like options'

rm -f -- -p
if ! run_img2sixel -a basic -J -p "${image_path}" \
        >"${json_out}" 2>"${shift_err}"; then
    echo 'img2sixel rejected assessment file named -p' >&2
    cat "${shift_err}" >&2 || :
    popd >/dev/null
    rm -f "${shift_err}" "${shift_out}" "${json_out}"
    exit 1
fi
if [[ ! -s -p ]]; then
    echo 'img2sixel did not create assessment file named -p' >&2
    popd >/dev/null
    rm -f "${shift_err}" "${shift_out}" "${json_out}"
    exit 1
fi
rm -f -- -p

popd >/dev/null

rm -f "${shift_err}" "${shift_out}" "${json_out}"
