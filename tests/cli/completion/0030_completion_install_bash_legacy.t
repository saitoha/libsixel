#!/bin/sh
# TAP test verifying bash legacy completion path selection.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

completion_home="${ARTIFACT_LOCAL_DIR}"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo '1..1'
set -v

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}",IMG2SIXEL_BASH_VERSION_OVERRIDE=3.2 \
    -- -2 bash >/dev/null || {
    fail 1 "legacy bash completion install failed"
    exit 0
}

test -f "${legacy_path}" || {
    fail 1 "legacy bash completion path is not created"
    exit 0
}

pass 1 "legacy bash completion path is used"
exit 0
