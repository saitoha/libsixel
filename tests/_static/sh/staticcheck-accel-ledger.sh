#!/bin/sh
# Verify that every SIMD and GPU implementation carries an Accel tag that
# agrees with the acceleration ledger, and that the ledger agrees back.
# Policy: docs/acceleration/README.md

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

echo "1..1"

src_root=$(CDPATH='' cd -- "$1" && pwd -P)
ledger="$src_root/docs/acceleration/ledger.md"
legacy="$src_root/tests/_static/data/accel-untagged-legacy.tsv"
scanner="$src_root/tests/_static/awk/accel-units.awk"
checker="$src_root/tests/_static/awk/accel-ledger.awk"
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-accel-ledger-XXXXXX")

# shellcheck disable=SC2329
cleanup() {
    rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

cd "$src_root"

# Vendored decoders keep their upstream SIMD paths and are not tracked.
sh "$source_files_helper" src --max-depth 0 \
    --name '*.c' --name '*.h' --name '*.m' --name '*.metal' \
    --name '*.glsl' --exclude-name 'stb_image.h' \
    --exclude-name 'stb_image_write.c' --exclude-name 'stb_image_write.h' |
    LC_ALL=C sort > "$tmpdir/sources"

# Only files that mention a vector type, an intrinsic or a kernel can hold a
# unit, so the scanner reads those and every GLSL program. The pattern is a
# superset of what accel-units.awk recognizes.
pattern='_mm(256)?_[a-z]|__m(128|256)|wasm_[a-z0-9]+_[a-z]|v128_t'
pattern=$pattern'|[0-9]x[0-9]+(x[0-9])?_t'
pattern=$pattern'|(^|[^A-Za-z0-9_])v[a-z0-9]+_(n_|lane_|laneq_|high_)?'
pattern=$pattern'[supf](8|16|32|64)'
pattern=$pattern'|kernel[[:space:]]+void|[[][[][[:space:]]*kernel'
grep '\.glsl$' "$tmpdir/sources" > "$tmpdir/candidates" || :
grep -v '\.glsl$' "$tmpdir/sources" | xargs grep -lE "$pattern" \
    >> "$tmpdir/candidates" || :
LC_ALL=C sort "$tmpdir/candidates" |
    xargs "${AWK:-awk}" -f "$scanner" > "$tmpdir/units"

"${AWK:-awk}" -v units="$tmpdir/units" -v legacy="$legacy" \
    -f "$checker" "$ledger" > "$tmpdir/errors" || {
    echo "not ok 1 - acceleration units agree with the ledger"
    LC_ALL=C sort "$tmpdir/errors" | sed 's/^/# /'
    exit 1
}

echo "ok 1 - acceleration units agree with the ledger"
exit 0
