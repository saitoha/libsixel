#!/bin/sh
# TAP test covering cli_token_is_known_option with short/long tokens.

set -eux

binary="${TOP_BUILDDIR}/tests/test_runner${SIXEL_BIN_EXT-}"
if [ ! -x "${binary}" ] && [ -z "${SIXEL_RUNTIME-}" ]; then
    echo "harness not built" >&2
    exit 99
fi

set +e
cli_output=$(${SIXEL_RUNTIME-} "${binary}" "cli/0029_cli_token_is_known_option" 2>&1)
rc=$?
set -e
printf '%s' "${cli_output}" >&2

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - cli_token_is_known_option"
else
    echo "not ok 1 - cli_token_is_known_option"
    exit 1
fi
