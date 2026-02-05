#!/bin/sh
# Shared helpers for mapfile TAP tests.

set -eu

mapfile_common_path=${mapfile_common_path:-"$0"}
mapfile_helper_dir=${MAPFILE_HELPER_DIR-}
if [ -z "${mapfile_helper_dir}" ]; then
    mapfile_helper_dir=$(CDPATH=; cd "$(dirname "${mapfile_common_path}")" && pwd)
fi
. "${mapfile_helper_dir}/../common/tap.sh"

# Initialize directories for artifacts and logs. The caller must pass the
# current test name to keep outputs isolated between TAP files.
setup_mapfile_dirs() {
:
}

# Load shared converter helpers and ensure the img2sixel binary is available
# for palette import/export tests.
load_mapfile_prereqs() {
    script_dir=$1
    helper_dir=${mapfile_helper_dir}

    . "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

    status=0
    ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

    snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
}
