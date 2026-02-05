#!/bin/sh
# TAP test comparing img2sixel --help with the manpage options list.

set -eux

help_opts="${ARTIFACT_LOCAL_DIR}/options-help.txt"
man_opts="${ARTIFACT_LOCAL_DIR}/options-man.txt"
help_sorted="${ARTIFACT_LOCAL_DIR}/options-help-sorted.txt"
man_sorted="${ARTIFACT_LOCAL_DIR}/options-man-sorted.txt"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

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

if run_img2sixel -H | awk '
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

if diff -u "${help_sorted}" "${man_sorted}"; then
    pass 1 "--help matches manpage"
else
    fail 1 "--help diverges from manpage"
fi

exit "${status}"
