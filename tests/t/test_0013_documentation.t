#!/bin/sh
# TAP test to keep img2sixel help, manpage, and shell completions in sync.

# Enable strict mode with verbose tracing for diagnostics.
set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/documentation.log"
help_opts="${artifact_dir}/options-help.txt"
man_opts="${artifact_dir}/options-man.txt"
bash_opts="${artifact_dir}/options-bash.txt"
zsh_opts="${artifact_dir}/options-zsh.txt"
help_sorted="${artifact_dir}/options-help-sorted.txt"
man_sorted="${artifact_dir}/options-man-sorted.txt"
bash_sorted="${artifact_dir}/options-bash-sorted.txt"
zsh_sorted="${artifact_dir}/options-zsh-sorted.txt"

mkdir -p "${artifact_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

die_skip() {
    reason=$1
    echo "1..3"
    echo "ok 1 - skip ${reason}"
    echo "ok 2 - skip ${reason}"
    echo "ok 3 - skip ${reason}"
    exit 0
}

require_file "${top_srcdir}/converters/img2sixel.1"
require_file "${top_srcdir}/converters/shell-completion/bash/img2sixel"
require_file "${top_srcdir}/converters/shell-completion/zsh/_img2sixel"

if ! command -v diff >/dev/null 2>&1; then
    die_skip "diff not available"
fi

echo "1..3"

if run_img2sixel -H 2>>"${log_file}" | awk '
    /^[[:space:]]*\*?-/ {
        line = $1
        sub(/^[[:space:]]*\*?/, "", line)
        split(line, parts, ",")
        print parts[1]
    }
' >"${help_opts}"; then
    :
else
    fail ${case_id} "failed to capture --help output"
fi

if LC_ALL=C sort "${help_opts}" >"${help_sorted}"; then
    :
else
    fail ${case_id} "failed to sort --help output"
fi

if awk '
    /^\.[ \t]*B[ \t]/ {
        field = $2
        gsub(/\\/, "", field)
        sub(/,.*/, "", field)
        if (field ~ /^-/) {
            print field
        }
    }
' "${top_srcdir}/converters/img2sixel.1" >"${man_opts}"; then
    :
else
    fail ${case_id} "failed to parse manpage"
fi

if LC_ALL=C sort "${man_opts}" >"${man_sorted}"; then
    :
else
    fail ${case_id} "failed to sort manpage options"
fi

if grep ' --[0-9a-zA-Z_@=~%?]' \
        "${top_srcdir}/converters/shell-completion/bash/img2sixel" \
        | grep -v "' " \
        | sed 's/.* \(-.\) .*/\1/' >"${bash_opts}"; then
    :
else
    fail ${case_id} "failed to parse bash completion"
fi

if LC_ALL=C sort "${bash_opts}" >"${bash_sorted}"; then
    :
else
    fail ${case_id} "failed to sort bash completion"
fi

if grep '{-' "${top_srcdir}/converters/shell-completion/zsh/_img2sixel" \
        | cut -f1 -d, \
        | cut -f2 -d'{' >"${zsh_opts}"; then
    :
else
    fail ${case_id} "failed to parse zsh completion"
fi

if LC_ALL=C sort "${zsh_opts}" >"${zsh_sorted}"; then
    :
else
    fail ${case_id} "failed to sort zsh completion"
fi

if diff -u "${help_sorted}" "${man_sorted}" >>"${log_file}" 2>&1; then
    pass ${case_id} "--help matches manpage"
else
    fail ${case_id} "--help diverges from manpage"
fi
case_id=$((case_id + 1))

if diff -u "${man_sorted}" "${bash_sorted}" >>"${log_file}" 2>&1; then
    pass ${case_id} "manpage matches bash completion"
else
    fail ${case_id} "manpage diverges from bash completion"
fi
case_id=$((case_id + 1))

if diff -u "${bash_sorted}" "${zsh_sorted}" >>"${log_file}" 2>&1; then
    pass ${case_id} "bash completion matches zsh completion"
else
    fail ${case_id} "bash completion diverges from zsh completion"
fi

exit "${status}"
