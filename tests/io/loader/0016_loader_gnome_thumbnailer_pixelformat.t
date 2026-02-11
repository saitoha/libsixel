#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

feature_defined_in_config "HAVE_FREEDESKTOP_THUMBNAILING" || {
    skip_all "gnome-thumbnailer loader is unavailable on this platform"
    exit 0
}

xdg_data_home="${TOP_SRCDIR}/tests/data/inputs/thumbnailer/cases/loader"
bin_dir="${TOP_SRCDIR}/tests/data/inputs/thumbnailer/cases/loader/bin"

echo "1..1"
set -v

run_test_runner --env "XDG_DATA_DIRS=${xdg_data_home}" \
                --env "PATH=${bin_dir}:${PATH}" \
                --env "HOME=${ARTIFACT_LOCAL_DIR}" \
                "loader/0016_loader_gnome_thumbnailer_pixelformat" || rc="$?"

test "${rc-}" = 77 && {
    echo "ok 1 - loader/0016_loader_gnome_thumbnailer_pixelformat # SKIP unavailable"
    exit 0
}

test -n "${rc-}" && {
    echo "not ok 1 - loader/0016_loader_gnome_thumbnailer_pixelformat"
    exit 0
}

echo "ok 1 - loader/0016_loader_gnome_thumbnailer_pixelformat"
exit 0
