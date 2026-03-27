#!/bin/sh
# TAP test for cli_option_requires_argument optstring parsing.

set -eux


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

rc=0
cli_output_file="${ARTIFACT_LOCAL_DIR}/cli_option_requires_argument.out"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cli/0030_cli_option_requires_argument" >"${cli_output_file}" 2>&1 || rc=$?
cat "${cli_output_file}" >&2 2>/dev/null || :

test "${rc}" -eq 0 || {
    echo "not ok" 1 - "cli_option_requires_argument"
    exit 0
}

echo "ok" 1 - "cli_option_requires_argument"
exit 0
