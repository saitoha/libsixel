#!/usr/bin/env bash
# Keep documentation and help output in sync.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

# ----------------------------------------------------------------------
#  +--------------------------+-------------------------------+
#  | Comparison               | Files                         |
#  +--------------------------+-------------------------------+
#  | help vs man page         | options1.txt vs options2.txt  |
#  | man page vs bash script  | options2.txt vs options3.txt  |
#  | bash vs zsh completions  | options3.txt vs options4.txt  |
#  +--------------------------+-------------------------------+
# ----------------------------------------------------------------------

tap_init "$(basename "$0")"

help_opts="${TMP_DIR}/options1.txt"
man_opts="${TMP_DIR}/options2.txt"
bash_opts="${TMP_DIR}/options3.txt"
zsh_opts="${TMP_DIR}/options4.txt"

generate_option_snapshots() {
    tap_log '[documentation] generating option snapshots'
    run_img2sixel -H | awk '
        /^[[:space:]]*\*?-/ {
            line = $1
            sub(/^[[:space:]]*\*?/, "", line)
            split(line, parts, ",")
            print parts[1]
        }
    ' >"${help_opts}"
    awk '
        /^\.B/ {
            field = $2
            if (field ~ /^\\/) {
                gsub(/\\/, "", field)
                gsub(/,/, "", field)
                print field
            }
        }
    ' "${SRC_DIR}/img2sixel.1" >"${man_opts}"
    grep ' --' "${SRC_DIR}/shell-completion/bash/img2sixel" | \
        grep -v "' " | sed 's/.* \(-.\) .*/\1/' >"${bash_opts}"
    grep '{-' "${SRC_DIR}/shell-completion/zsh/_img2sixel" | cut -f1 -d, | \
        cut -f2 -d'{' >"${zsh_opts}"
}

diff_option_lists() {
    local lhs
    local rhs

    lhs=$1
    rhs=$2
    tap_log "[documentation] diff ${lhs} vs ${rhs}"
    if command -v diff >/dev/null 2>&1; then
        diff "${lhs}" "${rhs}"
    else
        tap_diag "diff command unavailable; skipping comparison for ${lhs} vs ${rhs}."
    fi
}

generate_option_snapshots

tap_plan 3

tap_case 'help output matches man page' diff_option_lists "${help_opts}" "${man_opts}"
tap_case 'man page matches bash completion' diff_option_lists "${man_opts}" "${bash_opts}"
tap_case 'bash and zsh completions match' diff_option_lists "${bash_opts}" "${zsh_opts}"
