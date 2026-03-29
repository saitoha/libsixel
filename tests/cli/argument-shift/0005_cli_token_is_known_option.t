#!/bin/sh
# TAP test covering cli_token_is_known_option with short and long tokens.

set -eux


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

rc=0
cli_output_file="${ARTIFACT_LOCAL_DIR}/cli_token_is_known_option.out"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cli/0029_cli_token_is_known_option" >"${cli_output_file}" 2>&1 || rc=$?
cat "${cli_output_file}" >&2 2>/dev/null || :

test "${rc}" -eq 0 || {
    echo "not ok" 1 - "cli_token_is_known_option"
    exit 0
}

echo "ok" 1 - "cli_token_is_known_option"
exit 0
