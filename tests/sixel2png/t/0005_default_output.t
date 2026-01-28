#!/bin/sh
# TAP test verifying default output naming when -o/--output is omitted.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/default-output.log"
output_dir="${artifact_dir}/out"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



head_bytes() {
  n=$1; shift

  if head -c 0 </dev/null >/dev/null 2>&1; then
    head -c "$n" "$@"
    return
  fi

  if [ $# -gt 0 ] && [ "$1" != "-" ]; then
    dd if="$1" bs=1 count="$n" 2>/dev/null
  else
    dd bs=1 count="$n" 2>/dev/null
  fi
}

echo "1..1"
set -v

input_path="${images_dir}/snake.six"
require_file "${input_path}"

stdout_path="${output_dir}/stdout.png"
stderr_path="${output_dir}/stderr.txt"
rm -f "${stdout_path}" "${stderr_path}"

if run_sixel2png -i "${input_path}" >"${stdout_path}" 2>"${stderr_path}"; then
    if [ -s "${stdout_path}" ] \
            && [ "$(head_bytes 5 "${stdout_path}" | \
                LC_ALL=C od -An -tx1 | tr -d ' \n')" \
                = "89504e470d" ]; then
        pass 1 "default stdout PNG produced"
    else
        fail 1 "stdout png missing or invalid signature"
    fi
else
    fail 1 "sixel2png without -o failed"
fi

exit "${status}"
