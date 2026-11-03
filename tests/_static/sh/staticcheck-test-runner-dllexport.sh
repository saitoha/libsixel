#!/bin/sh
# Emit TAP for test_runner calls that require dllexport in visible headers.
# Policy: docs/testing/staticcheck.md

set -eux
LC_ALL=C
export LC_ALL
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

echo "1..1"
set -v

src_root=$1
header_file=$src_root/include/sixel.h.in
tests_root=$src_root/tests

if test ! -f "$header_file" || test ! -d "$tests_root"; then
    echo "ok 1 # SKIP missing include/sixel.h.in or tests directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-dllexport-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

decls_file=$tmpdir/decls.tsv
calls_file=$tmpdir/calls.tsv
missing_file=$tmpdir/missing.tsv
headers_file=$tmpdir/headers.txt
source_decls=$tmpdir/source-decls.tsv
calls_scanner=$tmpdir/calls.awk
# The sentinel detects accidentally truncating declarations in a later batch.
printf '%s\n' '# all declaration batches' > "$source_decls"

printf '%s\n' "$header_file" > "$headers_file"

# Include all owned headers: tests also reach them through support headers,
# relative includes and private implementation copies. Direct src/ includes
# alone miss those declarations. System compatibility declarations are not
# libsixel exports and are filtered by the symbol owner below.
sh "$source_files_helper" "$src_root/src" "$src_root/include" --name '*.h' >> "$headers_file"
LC_ALL=C sort -u "$headers_file" -o "$headers_file"

# The scanner keeps storage class with its declaration and skips function
# bodies. Static inline factories are local definitions, not DLL imports.
tr '\n' '\0' < "$headers_file" | xargs -0 awk \
    -f "$src_root/tests/_static/awk/dllexport-declarations.awk" \
    > "$decls_file"
LC_ALL=C sort -u "$decls_file" -o "$decls_file"

cat > "$calls_scanner" <<'AWK'
# Keep this scanner's state separate from dllexport-declarations.awk, which
# runs in the same process. In particular, its directive flag is a boolean.
function reset_calls(    key) {
    for (key in renamed) delete renamed[key]
    for (key in declaration_alias) delete declaration_alias[key]
}
function scan_calls(line, number,    token, name, count, parts) {
    if (line ~ /^[[:space:]]*#/) {
        if (line ~ /^[[:space:]]*#[[:space:]]*define[[:space:]]/) {
            sub(/^[[:space:]]*#[[:space:]]*define[[:space:]]+/, "", line)
            count = split(line, parts, /[[:space:]]+/)
            delete declaration_alias[renamed[parts[1]]]
            delete renamed[parts[1]]
            if (count == 2 && parts[1] ~ /^[A-Za-z_][A-Za-z0-9_]*$/ &&
                parts[2] ~ /^[A-Za-z_][A-Za-z0-9_]*$/) {
                renamed[parts[1]] = parts[2]
                declaration_alias[parts[2]] = parts[1]
            }
            return
        }
        if (line ~ /^[[:space:]]*#[[:space:]]*undef[[:space:]]/) {
            sub(/^[[:space:]]*#[[:space:]]*undef[[:space:]]+/, "", line)
            split(line, parts, /[[:space:]]+/)
            delete declaration_alias[renamed[parts[1]]]
            delete renamed[parts[1]]
            return
        }
    }
    while (match(line, /[A-Za-z_][A-Za-z0-9_]*[[:space:]]*\(/)) {
        token = substr(line, RSTART, RLENGTH)
        name = token
        sub(/[[:space:]]*\(.*/, "", name)
        if (!renamed[name] && name !~ /^(if|for|while|switch|return|sizeof)$/) {
            if (self_test) call_observed[name]++
            else printf "%s\t%s:%d\n", name, FILENAME, number
        }
        line = substr(line, RSTART + RLENGTH)
    }
}
BEGIN {
    # An alias suppresses only its own calls until undef or the next file;
    # declarations after directives must retain their DLL attribute.
    self_test = 1
    start_header("combined scanners")
    scan_line("#define sixel_hidden test_private", 1)
    scan_calls("#define sixel_hidden test_private", 1)
    scan_line("SIXEL_INTERNAL_API int sixel_visible(void);", 2)
    scan_calls("SIXEL_INTERNAL_API int sixel_visible(void);", 2)
    scan_calls("sixel_hidden();", 3)
    scan_calls("#undef sixel_hidden", 4)
    scan_calls("sixel_hidden();", 5)
    scan_calls("#define sixel_hidden test_private", 6)
    scan_line("int test_private(void);", 7)
    scan_calls("#define sixel_marked test_marked", 8)
    scan_line("SIXEL_INTERNAL_API int test_marked(void);", 9)
    scan_calls("#undef sixel_marked", 10)
    scan_line("int test_marked(void);", 11)
    start_header("next file")
    scan_line("SIXEL_INTERNAL_API int test_private(void);", 1)
    reset_calls()
    scan_calls("sixel_hidden();", 2)
    if (observed["sixel_visible"] != 1 ||
        !("sixel_hidden" in observed) || observed["sixel_hidden"] != 0 ||
        observed["sixel_marked"] != 1 ||
        !("test_marked" in observed) || observed["test_marked"] != 0 ||
        observed["test_private"] != 1 ||
        call_observed["sixel_visible"] != 1 ||
        call_observed["sixel_hidden"] != 2) {
        print "combined DLL scanner controls failed" > "/dev/stderr"
        exit 1
    }
    self_test = 0
}
FNR == 1 { reset_calls() }
/\(|^[[:space:]]*#/ { scan_calls($0, FNR) }
AWK
sh "$source_files_helper" "$tests_root" --name '*.[ch]' --exec awk \
    -v declarations_file="$source_decls" \
    -f "$src_root/tests/_static/awk/dllexport-declarations.awk" \
    -f "$calls_scanner" > "$calls_file"
LC_ALL=C sort -u "$calls_file" -o "$calls_file"

IFS= read -r first_declaration < "$source_decls"
if test "$first_declaration" != '# all declaration batches'; then
    echo "not ok 1 - DLL declaration batches were overwritten"
    exit 0
fi

if test ! -s "$calls_file"; then
    echo "ok 1 # SKIP no callable symbol references found in tests"
    exit 0
fi

awk '
BEGIN {
    FS = "\t"
}
FNR == NR {
    called[$1] = 1
    next
}
{
    name = $1
    line_no = $2
    exported = $3
    if (exported) visible[name] = 1
    if (called[name] && name ~ /^(sixel_|loader_|stbi_)/) {
        referenced[name] = line_no
    }
}
END {
    for (name in referenced) {
        if (!visible[name]) printf "%s\t%s\n", name, referenced[name]
    }
}
' "$calls_file" "$decls_file" > "$missing_file"

# A private implementation copy may rename symbols before including a C
# file. Its explicit prototypes must keep the original header DLL linkage.
awk -F '\t' '
FNR == NR { if ($3 == 1) exported[$1] = 1; next }
$3 == 0 && exported[$1] { print $1 "\t" $2 }
' "$decls_file" "$source_decls" >> "$missing_file"

if test ! -s "$missing_file"; then
    echo "ok 1 - test_runner header calls are dllexport-ready"
    exit 0
fi

echo "not ok 1 - test_runner header calls are dllexport-ready"
awk -F '\t' '{
    printf "# %s %s lacks SIXELAPI/SIXEL_INTERNAL_API\n", $2, $1
}' "$missing_file"
exit 0
