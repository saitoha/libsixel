#!/bin/sh
# TAP test ensuring png: prefix writes to filesystem path.

set -euxv

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

require_file "${images_dir}/snake.six"

echo "1..1"

prefixed_dir="${output_dir}/s2p-prefix"
rm -rf "${prefixed_dir}"
mkdir -p "${prefixed_dir}"
if run_sixel2png -o "png:${prefixed_dir}/out.png" \
        <"${images_dir}/snake.six" 2>>"${log_file}"; then
    if [ -s "${prefixed_dir}/out.png" ]; then
        cli_core_pass 1 "prefixed output trims scheme"
    else
        cli_core_fail 1 "prefixed output missing"
    fi
else
    cli_core_fail 1 "prefixed output command failed"
fi

exit "${status}"
