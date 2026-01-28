#!/bin/sh
# TAP test comparing img2sixel --help with the manpage options list.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/documentation.log"
help_opts="${artifact_dir}/options-help.txt"
man_opts="${artifact_dir}/options-man.txt"
help_sorted="${artifact_dir}/options-help-sorted.txt"
man_sorted="${artifact_dir}/options-man-sorted.txt"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



die_skip() {
    reason=$1
    echo "1..1"
    echo "ok 1 - skip ${reason}"
    exit 0
}

require_file "${top_srcdir}/converters/img2sixel.1"

if ! command -v diff >/dev/null 2>&1; then
    die_skip "diff not available"
fi

printf '1..1\n'
set -v

if run_img2sixel -H 2>>"${log_file}" | awk '
    /^[[:space:]]*\*?-/ {
        for (idx = 1; idx <= NF; idx++) {
            field = $idx
            sub(/^[[:space:]]*\*?/, "", field)
            gsub(/,/, "", field)
            if (field ~ /^--/) {
                sub(/\[.*/, "", field)
                sub(/=.*/, "", field)
            }
            if (field ~ /^-/ && field != "-") {
                print field
            }
        }
    }
' >"${help_opts}"; then
    :
else
    fail 1 "failed to capture --help output"
fi

if LC_ALL=C sort "${help_opts}" >"${help_sorted}"; then
    :
else
    fail 1 "failed to sort --help output"
fi

if awk '
    /^\.[ \t]*B[ \t]/ {
        for (idx = 2; idx <= NF; idx++) {
            field = $idx
            gsub(/\\/, "", field)
            gsub(/,/, "", field)
            if (field ~ /^--/) {
                sub(/=.*/, "", field)
            }
            if (field ~ /^-/ && field != "-") {
                print field
            }
        }
    }
' "${top_srcdir}/converters/img2sixel.1" >"${man_opts}"; then
    :
else
    fail 1 "failed to parse manpage"
fi

if LC_ALL=C sort "${man_opts}" >"${man_sorted}"; then
    :
else
    fail 1 "failed to sort manpage options"
fi

if diff -u "${help_sorted}" "${man_sorted}" >>"${log_file}" 2>&1; then
    pass 1 "--help matches manpage"
else
    fail 1 "--help diverges from manpage"
fi

exit "${status}"
