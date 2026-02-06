#!/bin/sh
# TAP test converting map64.six using explicit stdin/stdout arguments.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png - - <"${images_dir}/map64.six" \
        >"${ARTIFACT_LOCAL_DIR}/map64-stdin-stdout.png" || {
    fail 1 "map64 stdin/stdout conversion failed"
    exit 0
}

pass 1 "converts map64 with explicit stdin/stdout"
exit 0
