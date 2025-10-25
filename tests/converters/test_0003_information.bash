#!/usr/bin/env bash
# Ensure informational commands succeed.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

tap_init "$(basename "$0")"
tap_plan 1

if {
    tap_log '[test3] print information'

    # Confirm help output is accessible.
    run_img2sixel -H
    # Confirm version output is accessible.
    run_img2sixel -V
} >>"${TAP_LOG_FILE}" 2>&1; then
    tap_ok 1 'img2sixel informational commands succeed'
else
    tap_not_ok 1 'img2sixel informational commands succeed' \
        "See $(tap_log_hint) for details."
fi
