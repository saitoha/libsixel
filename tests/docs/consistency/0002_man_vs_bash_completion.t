#!/bin/sh
# TAP test comparing img2sixel manpage with bash completion definitions.

set -eux

man_opts="${ARTIFACT_LOCAL_DIR}/options-man.txt"
bash_opts="${ARTIFACT_LOCAL_DIR}/options-bash.txt"
man_sorted="${ARTIFACT_LOCAL_DIR}/options-man-sorted.txt"
bash_sorted="${ARTIFACT_LOCAL_DIR}/options-bash-sorted.txt"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



die_skip() {
    reason=$1
    echo "1..1"
    echo "ok 1 - skip ${reason}"
    exit 0
}




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

if diff -u "${man_sorted}" "${bash_sorted}"; then
    pass 1 "manpage matches bash completion"
else
    fail 1 "manpage diverges from bash completion"
fi

exit "${status}"
