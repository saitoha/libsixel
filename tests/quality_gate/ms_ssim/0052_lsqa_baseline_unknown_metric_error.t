#!/bin/sh
# Verify parse error when baseline metric name is unknown.

set -eux


printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

image_ref="${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp"
err_file="${ARTIFACT_LOCAL_DIR}/lsqa_baseline_unknown_metric.err"

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "UNKNOWN:0.1" "${image_ref}" >/dev/null 2>"${err_file}"
status=$?
set -e

test "${status}" -eq 2 || {
    echo "not ok" 1 - "unknown baseline metric was not rejected as expected"
    exit 0
}

err_match=0
while IFS= read -r line; do
    case "${line}" in
        *"Unknown metric name"*)
            err_match=1
            break
            ;;
    esac
done < "${err_file}"

test "${err_match}" -eq 1 || {
    echo "not ok" 1 - "unknown baseline metric was not rejected as expected"
    exit 0
}

echo "ok" 1 - "unknown baseline metric was rejected"

exit 0
