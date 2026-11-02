#!/bin/sh
# Public converter code must not reach into private library headers.
set -eux
echo '1..1'
set -v
src_root=$1
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/converters" \
    --name '*.c' --name '*.h' --name '*.m' --name '*.mm' \
    --exec awk -v root="$src_root" '
function exists(path,    status, unused) {
    status = (getline unused < path)
    close(path)
    return status >= 0
}
/^[[:space:]]*#[[:space:]]*include[[:space:]]+"/ {
    target = $0
    sub(/^[^"]*"/, "", target); sub(/".*/, "", target)
    directory = FILENAME; sub(/[^\/]*$/, "", directory)
    if (target ~ /^(\.\.\/)?src\// ||
        (target !~ /\// && exists(root "/src/" target) &&
         !exists(directory target))) {
        print FILENAME ":" FNR ": private header in public converter: " target
        failed = 1
    }
}
END { exit failed }
' >&2 || {
    echo 'not ok 1 - private include check'
    exit 0
}
echo 'ok 1 - private include check'
exit 0
