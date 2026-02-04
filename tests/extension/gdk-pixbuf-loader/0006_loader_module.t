#!/bin/sh
# TAP wrapper that dispatches to the unified C test runner.

set -eu

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
parent_dir=$(CDPATH=; cd "${script_dir}/../.." && pwd)

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_feature_available "HAVE_GDK_PIXBUF2" "gdk_pixbuf_loader" \
    "gdk-pixbuf loader"

if [ -n "${MESON_BUILD_ROOT:-}" ]; then
    top_builddir=${TOP_BUILDDIR:-${MESON_BUILD_ROOT}}
else
    top_builddir=${TOP_BUILDDIR:-${parent_dir}/..}
fi

runner="${top_builddir}/tests/test_runner"
if [ ! -x "${runner}" ]; then
    echo "Bail out! missing test binary: ${runner}" 1>&2
    exit 1
fi

test_name=$(basename "$0" .t)
log_file=$(mktemp "${TMPDIR:-/tmp}/gdk-pixbuf-loader-${test_name}.XXXXXX")
trap 'rm -f "${log_file}"' EXIT

set +e
"${runner}" "gdk-pixbuf-loader/${test_name}" >"${log_file}" 2>&1
rc=$?
set -e

echo "1..1"
set -v

if [ "${rc}" -eq 0 ]; then
    echo "ok 1 - gdk-pixbuf-loader/${test_name}"
elif [ "${rc}" -eq 77 ]; then
    echo "ok 1 - gdk-pixbuf-loader/${test_name} # SKIP unavailable"
else
    echo "not ok 1 - gdk-pixbuf-loader/${test_name}"
    sed 's/^/# /' "${log_file}"
    exit 1
fi
