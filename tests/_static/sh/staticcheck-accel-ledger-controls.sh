#!/bin/sh
# Verify that the acceleration ledger check rejects every kind of drift.
# Policy: docs/acceleration/README.md
set -eu

echo '1..1'
src_root=$(CDPATH='' cd -- "$1" && pwd -P)
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-accel-controls-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
root=$tmpdir/root
mkdir -p "$root/src" "$root/docs/acceleration" "$root/tests/_static/awk" \
    "$root/tests/_static/data"
cp "$src_root/tests/_static/awk/accel-units.awk" \
    "$src_root/tests/_static/awk/accel-ledger.awk" "$root/tests/_static/awk/"

cat > "$root/docs/acceleration/ledger.md" <<'EOF'
<!-- accel-ledger -->

| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Metal | Vulkan | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A-ONE | Tagged on several backends | done | done | n/a | done | planned | done | done | |
| A-TWO | Only planned | done | planned | planned | planned | planned | n/a | n/a | |
| C-BAD | Unknown state | done | maybe | n/a | n/a | n/a | n/a | n/a | |

<!-- accel-ledger-end -->
EOF
printf '%s\t%s\t%s\n' 'src/legacy.c' 1 old_loop 'src/legacy.c' 1 gone \
    > "$root/tests/_static/data/accel-untagged-legacy.tsv"

# Accepted: a tagged NEON function, an AVX2 function whose 128-bit helpers
# count as AVX2, a tagged GLSL program, prose that names a vector type, and
# a vendored decoder that is never scanned.
cat > "$root/src/tagged.c" <<'EOF'
/* Accel: A-ONE neon */
static void
tagged(void)
{
    uint8x16_t v = vdupq_n_u8(0);
}

/* uint8x16_t in a comment and "_mm_add_epi8" in a string are not code. */
static char const *prose = "_mm_add_epi8";

/* Accel: A-ONE avx2 */
static void
wide(void)
{
    __m256i a = _mm256_setzero_si256();
    __m128i b = _mm_setzero_si128();
}
EOF
printf '%s\n' '/* SPDX-License-Identifier: MIT */' '/* Accel: A-ONE vulkan */' \
    '#version 450' 'void main() {}' > "$root/src/program.glsl"
printf '%s\n' 'static void s(void) { __m128i v; }' > "$root/src/stb_image.h"

# Rejected: each file holds one kind of drift.
cat > "$root/src/drift.c" <<'EOF'
static void
untagged(void)
{
    uint8x16_t v;
}

/* Accel: A-ONE neon */
static void
mismatch(void)
{
    __m128i v;
}

/* Accel: A-TWO neon */
static void
planned(void)
{
    uint8x16_t v;
}

/* Accel: B-NONE neon */
static void
unknown(void)
{
    uint8x16_t v;
}

/* Accel: bad neon */
static void
malformed(void)
{
    uint8x16_t v;
}
EOF
cat > "$root/src/legacy.c" <<'EOF'
static void
old_loop(void)
{
    uint8x16_t v;
}

static void
old_loop(int n)
{
    uint8x16_t v;
}
EOF
printf '%s\n' 'kernel void lone(device uint *x [[buffer(0)]]) {}' \
    > "$root/src/lone.metal"

status=0
sh "$src_root/tests/_static/sh/staticcheck-accel-ledger.sh" "$root" \
    > "$tmpdir/actual" || status=$?
test "$status" -eq 1 || {
    echo 'not ok 1 - acceleration ledger drift must fail the check'
    exit 0
}
cat > "$tmpdir/expected" <<'EOF'
1..1
not ok 1 - acceleration units agree with the ledger
# ledger: A-ONE metal is done but no Accel tag names it
# ledger: C-BAD neon has unknown state "maybe"
# legacy: src/legacy.c gone is no longer untagged; lower or remove its row
# src/drift.c:1 untagged (neon): acceleration without an Accel tag
# src/drift.c:13 planned: Accel tag A-TWO neon but the ledger says planned
# src/drift.c:20 unknown: Accel tag B-NONE is not in the ledger
# src/drift.c:27 malformed (neon): acceleration without an Accel tag
# src/drift.c:28: malformed Accel tag
# src/drift.c:6 mismatch: uses sse2 but its Accel tag names neon
# src/legacy.c:6 old_loop (neon): more untagged units than the legacy list allows
# src/lone.metal:1 lone (metal): acceleration without an Accel tag
EOF
if cmp -s "$tmpdir/expected" "$tmpdir/actual"; then
    echo 'ok 1 - acceleration ledger drift fails the check'
else
    echo 'not ok 1 - acceleration ledger drift fails the check'
    diff "$tmpdir/expected" "$tmpdir/actual" | sed 's/^/# /' || :
fi
exit 0
