#!/bin/sh
# Policy: docs/testing/guide.md
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root" \
    --name Makefile.am --name meson.build --name '*.sh' --name '*.sh.in' --name '*.t' --name '*.py' \
    --exclude-path '*/data/*' --exclude-path '*/third_party/*' \
    --exclude-name ltmain.sh --exec awk -v root="$src_root" \
    -f "$src_root/tests/_static/awk/shell-words.awk" \
    -f "$src_root/tests/_static/awk/no-find.awk" >&2 || {
    echo 'not ok 1 - build and test code does not depend on findutils'
    exit 0
}
echo 'ok 1 - build and test code does not depend on findutils'
exit 0
