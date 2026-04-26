#!/bin/sh
# Emit TAP for duplicate static function symbols across amalgamation units.

set -eu

echo "1..1"

src_root=$1
src_makefile=$src_root/src/Makefile.am
converters_makefile=$src_root/converters/Makefile.am

if test ! -f "$src_makefile"; then
    echo "ok 1 # SKIP missing src/Makefile.am"
    exit 0
fi
if test ! -f "$converters_makefile"; then
    echo "ok 1 # SKIP missing converters/Makefile.am"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-static-symbols-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

unit_entries=$tmpdir/unit_entries.txt
unit_files=$tmpdir/unit_files.txt
raw_symbols=$tmpdir/raw_symbols.tsv
symbols=$tmpdir/symbols.tsv
duplicates=$tmpdir/duplicates.tsv

awk -v src_base="$src_root/src" '
{
    line = $0
    sub(/#.*/, "", line)
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.\/-]+\.[cm]/)) {
        token = substr(line, RSTART, RLENGTH)
        sub(/^\$\(srcdir\)\//, "", token)
        print src_base "/" token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$src_makefile" > "$unit_entries"

awk -v src_base="$src_root/converters" '
{
    line = $0
    sub(/#.*/, "", line)
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.\/-]+\.[cm]/)) {
        token = substr(line, RSTART, RLENGTH)
        sub(/^\$\(srcdir\)\//, "", token)
        print src_base "/" token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$converters_makefile" >> "$unit_entries"

LC_ALL=C sort -u "$unit_entries" -o "$unit_entries"

if awk '
/SIXEL_AMALGAMATION_SPLIT_DITHER_POLICY=1/ {
    found = 1
}
END {
    exit found ? 0 : 1
}
' "$src_makefile"; then
    awk '
{
    is_dither_policy = (index($0, "/src/dither-policy-") > 0)
    is_c_file = ($0 ~ /\.c$/)
    if (!(is_dither_policy && is_c_file)) {
        print
    }
}
' "$unit_entries" > "$tmpdir/unit_entries.filtered"
    mv "$tmpdir/unit_entries.filtered" "$unit_entries"
fi

if test ! -s "$unit_entries"; then
    echo "ok 1 # SKIP no C/ObjC source entries found"
    exit 0
fi

: > "$unit_files"
while IFS= read -r unit_path; do
    test -n "$unit_path" || continue
    case "$unit_path" in *.c|*.m) ;; *) continue ;; esac
    test -f "$unit_path" || continue
    printf '%s\n' "$unit_path" >> "$unit_files"
done < "$unit_entries"

if test ! -s "$unit_files"; then
    echo "ok 1 # SKIP no C/ObjC amalgamation units found"
    exit 0
fi

: > "$raw_symbols"
while IFS= read -r unit_path; do
    awk -v file="$unit_path" '
function trim(s) {
    sub(/^[[:space:]]+/, "", s)
    sub(/[[:space:]]+$/, "", s)
    return s
}
function strip_comments(s,   out, seg, p0, p1) {
    out = ""
    seg = s
    while (1) {
        if (in_comment) {
            p1 = index(seg, "*/")
            if (p1 == 0) {
                return out
            }
            seg = substr(seg, p1 + 2)
            in_comment = 0
            continue
        }
        p0 = index(seg, "/*")
        p1 = index(seg, "//")
        if (p1 > 0 && (p0 == 0 || p1 < p0)) {
            out = out substr(seg, 1, p1 - 1)
            return out
        }
        if (p0 == 0) {
            out = out seg
            return out
        }
        out = out substr(seg, 1, p0 - 1)
        seg = substr(seg, p0 + 2)
        in_comment = 1
    }
}
function count_open(s, t) {
    t = s
    return gsub(/\(/, "", t)
}
function count_close(s, t) {
    t = s
    return gsub(/\)/, "", t)
}
function reset_state() {
    state = 0
    name = ""
    depth = 0
}
function start_signature(line) {
    if (!match(line, /^[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        return 0
    }
    name = substr(line, RSTART, RLENGTH)
    sub(/[[:space:]]*\($/, "", name)
    depth = count_open(line) - count_close(line)
    if (line ~ /;[[:space:]]*$/ && depth <= 0) {
        reset_state()
        return 0
    }
    if (depth <= 0) {
        if (line ~ /\{/) {
            printf "%s\t%s\n", name, file
            reset_state()
            return 0
        }
        state = 3
        return 1
    }
    state = 2
    return 1
}
BEGIN {
    in_comment = 0
    reset_state()
}
{
    line = trim(strip_comments($0))
    if (line == "") {
        next
    }

    if (state == 0) {
        if (line ~ /^static([[:space:]]|$)/) {
            if (line ~ /;[[:space:]]*$/ && line !~ /\(/) {
                next
            }
            if (sub(/^static([[:space:]]+inline)?[[:space:]]+/, "", line)) {
                if (line ~ /=/ || line ~ /\[/ ||
                    (line ~ /\{/ && line !~ /\(/)) {
                    reset_state()
                    next
                }
                if (start_signature(line)) {
                    next
                }
            }
            state = 1
        }
        next
    }

    if (state == 1) {
        if (line ~ /=/ || line ~ /\[/ ||
            (line ~ /\{/ && line !~ /\(/)) {
            reset_state()
            next
        }
        if (line ~ /;[[:space:]]*$/ && line !~ /\(/) {
            reset_state()
            next
        }
        if (start_signature(line)) {
            next
        }
        next
    }

    if (state == 2) {
        depth += count_open(line) - count_close(line)
        if (depth > 0) {
            next
        }
        if (line ~ /;[[:space:]]*$/) {
            reset_state()
            next
        }
        if (line ~ /\{/) {
            printf "%s\t%s\n", name, file
            reset_state()
            next
        }
        state = 3
        next
    }

    if (state == 3) {
        if (line ~ /;[[:space:]]*$/) {
            reset_state()
            next
        }
        if (line ~ /\{/) {
            printf "%s\t%s\n", name, file
            reset_state()
            next
        }
    }
}
' "$unit_path" >> "$raw_symbols"
done < "$unit_files"

if test ! -s "$raw_symbols"; then
    echo "ok 1 - amalgamation static symbols are unique"
    exit 0
fi

LC_ALL=C sort -u "$raw_symbols" > "$symbols"

if awk -F '\t' '
{
    symbol = $1
    file = $2
    if (!(symbol in order)) {
        count += 1
        order[symbol] = count
        names[count] = symbol
        files[symbol] = file
        seen[symbol SUBSEP file] = 1
        symbol_count[symbol] = 1
        next
    }
    if ((symbol SUBSEP file) in seen) {
        next
    }
    seen[symbol SUBSEP file] = 1
    symbol_count[symbol] += 1
    files[symbol] = files[symbol] ", " file
}
END {
    status = 0
    for (i = 1; i <= count; ++i) {
        symbol = names[i]
        if (symbol_count[symbol] > 1) {
            status = 1
            printf "%s\t%d\t%s\n", symbol, symbol_count[symbol], files[symbol]
        }
    }
    exit status
}
' "$symbols" > "$duplicates"; then
    echo "ok 1 - amalgamation static symbols are unique"
    exit 0
fi

echo "not ok 1 - amalgamation static symbols are unique"
while IFS=$(printf '\t') read -r symbol count files; do
    test -n "$symbol" || continue
    echo "# duplicate static symbol: $symbol ($count definitions)"
    echo "# files: $files"
done < "$duplicates"
exit 1
