#!/usr/bin/env bash
# Keep documentation and help output in sync.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# shellcheck source=tests/converters/common.bash
source "${SCRIPT_DIR}/common.bash"

echo '[test14] documentation'

help_opts="${TMP_DIR}/options1.txt"
man_opts="${TMP_DIR}/options2.txt"
bash_opts="${TMP_DIR}/options3.txt"
zsh_opts="${TMP_DIR}/options4.txt"

run_img2sixel -H | awk '
    /^[[:space:]]*\*?-/ {
        line = $1
        sub(/^[[:space:]]*\*?/, "", line)
        split(line, parts, ",")
        print parts[1]
    }
' > "${help_opts}"
awk '
    /^\.B/ {
        field = $2
        if (field ~ /^\\/) {
            gsub(/\\/, "", field)
            gsub(/,/, "", field)
            print field
        }
    }
' "${SRC_DIR}/img2sixel.1" > "${man_opts}"
grep ' --' "${SRC_DIR}/shell-completion/bash/img2sixel" | grep -v "' " | \
    sed 's/.* \(-.\) .*/\1/' > "${bash_opts}"
grep '{-' "${SRC_DIR}/shell-completion/zsh/_img2sixel" | cut -f1 -d, | cut -f2 -d'{' > "${zsh_opts}"

if command -v diff >/dev/null 2>&1; then
    diff "${help_opts}" "${man_opts}"
    diff "${man_opts}" "${bash_opts}"
    diff "${bash_opts}" "${zsh_opts}"
fi
