#!/bin/sh
# Emit TAP for src/*.c config include order.

set -eu

echo "1..1"

src_root=$1
src_dir=$src_root/src

if test ! -d "$src_dir"; then
    echo "ok 1 # SKIP missing src directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-src-config-order-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

missing=$tmpdir/missing.txt
status=0

for path in "$src_dir"/*.c; do
    test -f "$path" || continue

    result=$(awk '
    BEGIN {
        first_include = ""
        has_config = 0
    }
    /^[[:space:]]*#[[:space:]]*include[[:space:]]+"config.h"/ {
        has_config = 1
    }
    /^[[:space:]]*#[[:space:]]*include[[:space:]]+/ {
        if (first_include == "") {
            first_include = $0
        }
    }
    END {
        if (first_include == "") {
            print "NO_INCLUDE"
            exit 0
        }
        if (first_include !~ /"config\.h"/) {
            print "FIRST_NOT_CONFIG"
            exit 0
        }
        if (has_config == 0) {
            print "MISSING_CONFIG"
            exit 0
        }
        print "OK"
    }
    ' "$path")

    case "$result" in
        OK)
            ;;
        NO_INCLUDE)
            echo "# ${path##*/}: missing include directives" >> "$missing"
            status=1
            ;;
        MISSING_CONFIG)
            echo "# ${path##*/}: missing config.h include" \
                >> "$missing"
            status=1
            ;;
        FIRST_NOT_CONFIG)
            echo "# ${path##*/}: first include must be config.h" \
                >> "$missing"
            status=1
            ;;
        *)
            echo "# ${path##*/}: unexpected check state: $result" >> "$missing"
            status=1
            ;;
    esac
done

if test "$status" -eq 0; then
    echo "ok 1 - src c files include config.h first"
    exit 0
fi

echo "not ok 1 - src c files include config.h first"
cat "$missing"
exit 1
