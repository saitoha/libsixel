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

    for spec in \
        classid-factory:sixel_factory_classid_hash:sixel_factory_classid_wordlist \
        classid-service:sixel_components_serviceid_hash:sixel_components_serviceid_wordlist
    do
        base=${spec%%:*}
        rest=${spec#*:}
        hash_name=${rest%%:*}
        wordlist_name=${rest#*:}
        header=$src_dir/$base.h

        test -f "$header" || continue
        awk -v label="src/$base.h" \
            -v hash_name="$hash_name" \
            -v wordlist_name="$wordlist_name" '
        $0 ~ "^[[:space:]]*" hash_name "[[:space:]]*\\(" {
            saw_hash = 1
        }
        $0 ~ "^[[:space:]]*hash[[:space:]]*\\(" {
            saw_generic_hash = 1
        }
        $0 ~ "^[[:space:]]*static const .* " wordlist_name "\\[\\]" {
            saw_wordlist = 1
        }
        $0 ~ "^[[:space:]]*static const .* wordlist\\[\\]" {
            saw_generic_wordlist = 1
        }
        END {
            failed = 0
            if (!saw_hash) {
                printf("%s missing generated hash function %s\n",
                       label, hash_name)
                failed = 1
            }
            if (saw_generic_hash) {
                printf("%s uses generic generated hash function name\n",
                       label)
                failed = 1
            }
            if (!saw_wordlist) {
                printf("%s missing generated wordlist %s\n",
                       label, wordlist_name)
                failed = 1
            }
            if (saw_generic_wordlist) {
                printf("%s uses generic generated wordlist name\n",
                       label)
                failed = 1
            }
            exit failed ? 1 : 0
        }
        ' "$header" || :
    done
)

if test -n "$missing"; then
    echo "not ok 1 - gperf generated headers clean up fixed macros"
    printf '%s\n' "$missing" | sed 's/^/# /'
    exit 1
fi

echo "ok 1 - gperf generated headers clean up fixed macros"
