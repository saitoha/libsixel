#!/bin/sh
# TAP test verifying bash legacy completion path selection.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo '1..1'
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

completion_home="${ARTIFACT_LOCAL_DIR}"
legacy_path="${completion_home}/.bash_completion.d/img2sixel"

run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}" \
              --env IMG2SIXEL_BASH_VERSION_OVERRIDE=3.2 \
              -2 bash >&2 || {
    echo "not ok" 1 - "legacy bash completion install failed"
    exit 0
}

test -f "${legacy_path}" || {
    echo "not ok" 1 - "legacy bash completion path is not created"
    exit 0
}

echo "ok" 1 - "legacy bash completion path is used"
exit 0
