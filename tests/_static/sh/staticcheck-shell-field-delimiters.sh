#!/bin/sh
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
checker=$src_root/tests/_static/awk/shell-field-delimiters.awk
# shellcheck disable=SC2016
# Feed literal controls directly; no interpreter is needed to run the scripts.
printf '%s\n' 'field=${value%%|*}' | awk -f "$checker" >&2 && {
    echo 'not ok 1 - unescaped delimiter escaped detection'
    exit 0
}
# shellcheck disable=SC2016
printf '%s\n' 'field=${value%%\|*}' 'field=${value%%"|"*}' \
    '# field=${value%%|*}' | awk -f "$checker" >&2 || {
    echo 'not ok 1 - escaped delimiters or comments were rejected'
    exit 0
}
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.t' --exclude-path '*/data/*' --exclude-path '*/_artifacts/*' \
    --exec awk -f "$checker" >&2 || {
    echo 'not ok 1 - portable shell field delimiters'
    exit 0
}
echo 'ok 1 - portable shell field delimiters'
exit 0
