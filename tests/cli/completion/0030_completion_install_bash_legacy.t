#!/bin/sh
# TAP test verifying bash legacy completion path selection.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
completion_home=""
legacy_path=""

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

if command -v mktemp >/dev/null 2>&1; then
    completion_home=$(mktemp -d "${TMPDIR:-/tmp}/img2sixel-home.XXXXXX")
else
    completion_home="${ARTIFACT_LOCAL_DIR}/home-legacy.$$"
fi

if [ -z "${completion_home}" ]; then
    echo "Failed to create a temporary home directory" >&2
    exit 1
fi

cleanup_home() {
    rm -rf "${completion_home}"
}
trap cleanup_home EXIT INT TERM

legacy_path="${completion_home}/.bash_completion.d/img2sixel"

echo '1..1'
set -v

if run_img2sixel --env IMG2SIXEL_COMPLETION_HOME="${completion_home}",BASH_VERSION=3.2 \
        -- -2 bash >/dev/null; then
    if [ -f "${legacy_path}" ]; then
        pass 1 "legacy bash completion path is used"
    else
        fail 1 "legacy bash completion path is not created"
    fi
else
    fail 1 "legacy bash completion install failed"
fi

exit "${status}"
