#!/bin/sh
# TAP test ensuring png: prefix writes to filesystem path.

set -eux

CLI_CORE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/cli-core"
. "${CLI_CORE_HELPER_DIR}/cli_core_common.sh"
cli_core_setup "sixel2png-basic"

ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"



echo "1..1"
set -v

prefixed_dir="${ARTIFACT_LOCAL_DIR}"
rm -f "${prefixed_dir}/out.png"
if run_sixel2png -o "png:${prefixed_dir}/out.png" \
        <"${images_dir}/snake.six"; then
    if [ -s "${prefixed_dir}/out.png" ]; then
        cli_core_pass 1 "prefixed output trims scheme"
    else
        cli_core_fail 1 "prefixed output missing"
    fi
else
    cli_core_fail 1 "prefixed output command failed"
fi

exit "${status}"
