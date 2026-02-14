#!/bin/sh
# TAP test ensuring sixel2png rejects invalid thread tokens.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

threads_err="${ARTIFACT_LOCAL_DIR}/err.txt"

run_sixel2png -= bogus <"${images_dir}/map64.six" \
        >"${ARTIFACT_LOCAL_DIR}/capture.$$" 2> "${threads_err}" && {
    fail 1 "accepts invalid thread token"
    exit 0
}

grep "threads must be a positive integer or 'auto'" "${threads_err}" >/dev/null || {
    fail 1 "missing invalid thread diagnostic"
    exit 0
}

pass 1 "rejects invalid thread token"
exit 0
