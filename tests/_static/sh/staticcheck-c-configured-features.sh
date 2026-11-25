#!/bin/sh
# New platform/compiler guesses and arithmetic behavior probes belong in
# configure/setup. Standard C availability feature macros are permitted.
set -eux
echo '1..1'
set -v
src_root=$1
LC_ALL=C
export LC_ALL
observed=$(mktemp "${TMPDIR:-/tmp}/libsixel-configured-features-XXXXXX")
trap 'rm -f "$observed"' EXIT HUP INT TERM
sh "$src_root/tests/_static/sh/list-source-files.sh" \
    "$src_root/src" "$src_root/include" "$src_root/converters" \
    "$src_root/assessment" "$src_root/tests" "$src_root/tools" \
    "$src_root/examples" "$src_root/fuzz" \
    --name '*.c' --name '*.h' --name '*.h.in' --name '*.m' \
    --name '*.cc' --name '*.cpp' --name '*.mm' \
    --exclude-path '*/third_party/*' --exclude-path '*/data/*' \
    --exclude-path "$src_root/include/sixel.h" \
    --exclude-name 'stb_image.h' --exclude-name 'stb_image_write.h' \
    --exec awk -v root="$src_root" \
    -f "$src_root/tests/_static/awk/c-configured-features.awk" > "$observed" || {
    echo 'not ok 1 - C capability detection or builtin guard check failed'
    exit 0
}
awk -F '\t' '
    FNR == NR {
        if ($0 ~ /^#/ || $0 == "") next
        key = $1 SUBSEP $3
        if (NF != 3 || $2 !~ /^[1-9][0-9]*$/ || key in allowed) {
            print "# invalid configured-feature legacy entry: " FNR
            failed = 1
        }
        allowed[key] = $2
        next
    }
    {
        key = $1 SUBSEP $3
        if (++seen[key] > allowed[key]) {
            print "# " $1 ":" $2 ": move capability/behavior detection to configure.ac and meson.build: " $3
            failed = 1
        }
    }
    END {
        for (key in allowed) {
            if (seen[key] < allowed[key]) {
                split(key, entry, SUBSEP)
                print "# remove stale legacy decision: " entry[1] ": " entry[2]
                failed = 1
            }
        }
        exit failed
    }
' "$src_root/tests/_static/data/c-configured-features-legacy.tsv" "$observed" || {
    echo 'not ok 1 - C decisions use configured capabilities or standard availability macros'
    exit 0
}
echo 'ok 1 - C decisions use configured capabilities or standard availability macros'
exit 0
