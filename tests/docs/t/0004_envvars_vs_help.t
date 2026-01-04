#!/bin/sh
# TAP test verifying that img2sixel -H documents every environment variable
# referenced in the sources.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
repo_root=$(CDPATH=; cd "${test_dir}/../../.." && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/documentation.log"

mkdir -p "${artifact_dir}"

if [ -z "${TOP_SRCDIR:-}" ]; then
    TOP_SRCDIR=${repo_root}
fi

if [ -z "${TOP_BUILDDIR:-}" ] && [ -d "${repo_root}/build" ]; then
    TOP_BUILDDIR=${repo_root}/build
fi

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
require_file "${top_srcdir}/tests/docs/list_envvars.sh"

printf '1..1\n'

if run_quiet "${top_srcdir}/tests/docs/list_envvars.sh" --check \
        --img2sixel "${IMG2SIXEL_PATH}" --source-root "${top_srcdir}" \
        >"${log_file}" 2>&1; then
    printf 'ok 1 - environment variables match between sources and -H\n'
else
    printf 'not ok 1 - mismatch between sources and -H (see %s)\n' \
        "${log_file}"
    status=1
fi

exit "${status}"
