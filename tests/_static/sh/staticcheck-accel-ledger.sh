#!/bin/sh
# Verify that every SIMD and GPU implementation carries an Accel tag that
# agrees with the acceleration ledger, and that the ledger agrees back.
# Policy: docs/acceleration/README.md

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
ledger="$src_root/docs/acceleration/ledger.md"
scanner="$src_root/tests/_static/awk/accel-units.awk"
checker="$src_root/tests/_static/awk/accel-ledger.awk"
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-accel-ledger-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

cd "$src_root"

# Inventory maintained sources, including subdirectories. The stb files are
# upstream copies, and generated shader headers are covered at their inputs.
set -- src
for dir in assessment converters include; do
    if test -d "$dir"; then
        set -- "$@" "$dir"
    fi
done
sh "$source_files_helper" "$@" --name '*.c' --name '*.h' \
    --name '*.inc' --name '*.m' --name '*.mm' --name '*.metal' \
    --name '*.glsl' --name '*.wgsl' --name '*.comp' \
    --exclude-path src/stb_image.h --exclude-path src/stb_image_write.c \
    --exclude-path src/stb_image_write.h --exclude-name '*-shader.inc.h' \
    --exclude-name '*-wgsl.inc.h' > "$tmpdir/sources"

# Keep the prefilter broader than the lexer. Shader fragments are included
# by suffix, even if they only contain declarations.
pattern='Accel:|_mm|__m(64|128|256|512)|wasm_|v128_t|[0-9]+x[0-9]+'
pattern=$pattern'|v[a-z0-9_]+_[supfb][0-9]+|__builtin_|vector_size|ext_vector_type'
# A shader keyword can be split across adjacent C string literals.
pattern=$pattern'|kern|ernel|@comp|ompute|gl_Global|lobalInvocation|local_size_x'
LC_ALL=C grep -E '\.(metal|glsl|wgsl|comp)$' "$tmpdir/sources" \
    > "$tmpdir/candidates" || test "$?" -eq 1
LC_ALL=C grep -vE '\.(metal|glsl|wgsl|comp)$' "$tmpdir/sources" \
    > "$tmpdir/host" || test "$?" -eq 1
if test -s "$tmpdir/host"; then
    tr '\n' '\000' < "$tmpdir/host" > "$tmpdir/host0"
    # The child shell receives the pattern as $0 and the file batch as $@.
    # shellcheck disable=SC2016
    LC_ALL=C xargs -0 sh -c 'grep -lE "$0" "$@" || test "$?" -eq 1' \
        "$pattern" < "$tmpdir/host0" >> "$tmpdir/candidates"
fi
: > "$tmpdir/units"
if test -s "$tmpdir/candidates"; then
    tr '\n' '\000' < "$tmpdir/candidates" > "$tmpdir/candidates0"
    LC_ALL=C xargs -0 "${AWK:-awk}" -f "$scanner" \
        < "$tmpdir/candidates0" > "$tmpdir/units"
fi

LC_ALL=C "${AWK:-awk}" -v units="$tmpdir/units" \
    -f "$checker" "$ledger" > "$tmpdir/errors" || {
    echo "not ok 1 - acceleration units agree with the ledger"
    LC_ALL=C sort "$tmpdir/errors" | sed 's/^/# /'
    exit 1
}

echo "ok 1 - acceleration units agree with the ledger"
exit 0
