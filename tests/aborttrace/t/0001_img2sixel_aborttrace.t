#!/bin/sh
# TAP test verifying img2sixel emits abort traces on SIGABRT.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_feature_available "SIXEL_ENABLE_ABORT_TRACE" "abort_trace" \
    "abort trace support"
ensure_img2sixel_available

echo "1..1"

stderr_file="${output_dir}/img2sixel-aborttrace.err"
output_file="${output_dir}/img2sixel-aborttrace.six"
fifo_path="${tmp_dir}/img2sixel-aborttrace.fifo"

rm -f "${fifo_path}"
mkfifo "${fifo_path}"

set +x
if [ -n "${SIXEL_RUNTIME:-}" ]; then
    SIXEL_ABORT_TRACE=1 "${SIXEL_RUNTIME}" "${IMG2SIXEL_PATH}" \
        <"${fifo_path}" >"${output_file}" 2>"${stderr_file}" &
    pid=$!
else
    SIXEL_ABORT_TRACE=1 "${IMG2SIXEL_PATH}" \
        <"${fifo_path}" >"${output_file}" 2>"${stderr_file}" &
    pid=$!
fi
set -x

exec 3>"${fifo_path}"

sleep 0.5
kill -ABRT "${pid}" || true

set +e
wait "${pid}"
set -e

exec 3>&-
rm -f "${fifo_path}"

if grep -F "libsixel: abort() detected" "${stderr_file}" >/dev/null \
        && grep -F "libsixel: abort trace complete" "${stderr_file}" \
            >/dev/null; then
    pass 1 "img2sixel abort trace emitted"
else
    fail 1 "img2sixel abort trace missing"
fi

exit "${status}"
