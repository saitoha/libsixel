#!/bin/sh
# TAP test comparing img2sixel manpage with bash completion definitions.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/documentation.log"
man_opts="${artifact_dir}/options-man.txt"
bash_opts="${artifact_dir}/options-bash.txt"
man_sorted="${artifact_dir}/options-man-sorted.txt"
bash_sorted="${artifact_dir}/options-bash-sorted.txt"

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
require_file "${top_srcdir}/converters/shell-completion/bash/img2sixel"

if ! command -v diff >/dev/null 2>&1; then
    die_skip "diff not available"
fi

printf '1..1\n'
set -v

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

if awk '
    function emit(opt) {
        sub(/[ \t]+$/, "", opt)
        sub(/\\$/, "", opt)
        if (opt ~ /^-/) {
            print opt
        }
    }
    /^[ \t]*-[^ \t]+[ \t]+--/ {
        emit($1)
        emit($2)
    }
' "${top_srcdir}/converters/shell-completion/bash/img2sixel" \
        | LC_ALL=C sort -u >"${bash_opts}"; then
    :
else
    fail 1 "failed to parse bash completion"
fi

if LC_ALL=C sort "${bash_opts}" >"${bash_sorted}"; then
    :
else
    fail 1 "failed to sort bash completion"
fi

if diff -u "${man_sorted}" "${bash_sorted}" >>"${log_file}" 2>&1; then
    pass 1 "manpage matches bash completion"
else
    fail 1 "manpage diverges from bash completion"
fi

exit "${status}"
