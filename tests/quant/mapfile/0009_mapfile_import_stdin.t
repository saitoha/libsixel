#!/bin/sh
# TAP test: palette input accepted from stdin.

set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
MAPFILE_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/mapfile"
. "${MAPFILE_HELPER_DIR}/mapfile_common.sh"

setup_mapfile_dirs "${test_name}"
load_mapfile_prereqs "${script_dir}"

echo "1..1"
set -v

gpl_palette="${ARTIFACT_LOCAL_DIR}/palette-gpl.dat"
if ! run_img2sixel -M gpl:"${gpl_palette}" -o "${ARTIFACT_LOCAL_DIR}/pal-gpl.six" \
        "${snake_png}"; then
    fail "Preparing GPL palette for stdin import failed"
    exit "${status}"
fi

if cat "${gpl_palette}" | run_img2sixel -m gpl:- \
        -o "${ARTIFACT_LOCAL_DIR}/from-stdin.six" "${snake_png}"; then
    if [ -s "${ARTIFACT_LOCAL_DIR}/from-stdin.six" ]; then
        pass "Palette input accepted from stdin"
    else
        fail "stdin palette conversion produced no data"
    fi
else
    fail "stdin palette conversion failed"
fi

exit "${status}"
