#!/bin/sh
# Emit TAP for gperf generated-header macro cleanup contracts.

set -eu

src_root=$1
src_dir=$src_root/src
makefile_am=$src_dir/Makefile.am
macros='TOTAL_KEYWORDS MIN_WORD_LENGTH MAX_WORD_LENGTH MIN_HASH_VALUE MAX_HASH_VALUE'

echo "1..1"

if test ! -f "$makefile_am"; then
    echo "ok 1 # SKIP missing src/Makefile.am"
    exit 0
fi

missing=$(
    for gperf_file in "$src_dir"/*.gperf; do
        test -f "$gperf_file" || continue
        base=${gperf_file##*/}
        base=${base%.gperf}
        header=$src_dir/$base.h

        if test ! -f "$header"; then
            printf 'missing generated header: src/%s.h\n' "$base"
            continue
        fi

        for macro in $macros; do
            awk -v macro="$macro" '
            $0 == "#undef " macro {
                found = 1
            }
            END {
                exit found ? 0 : 1
            }
            ' "$header" || \
                printf 'src/%s.h missing #undef %s\n' "$base" "$macro"

            awk -v target=">> \$(srcdir)/$base.h" \
                -v macro="#undef $macro" '
            /printf[[:space:]]/ {
                in_printf = 1
                saw_macro = 0
            }
            in_printf && index($0, macro) {
                saw_macro = 1
            }
            in_printf && index($0, target) {
                if (saw_macro) {
                    found = 1
                }
                in_printf = 0
                saw_macro = 0
            }
            END {
                exit found ? 0 : 1
            }
            ' "$makefile_am" || \
                printf 'src/Makefile.am does not append #undef %s to src/%s.h\n' \
                    "$macro" "$base"
        done
    done
)

if test -n "$missing"; then
    echo "not ok 1 - gperf generated headers clean up fixed macros"
    printf '%s\n' "$missing" | sed 's/^/# /'
    exit 1
fi

echo "ok 1 - gperf generated headers clean up fixed macros"
