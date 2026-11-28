#!/bin/sh
# Verify that the acceleration ledger check accepts tagged code and rejects drift.
# Policy: docs/acceleration/README.md
set -eu

echo '1..2'
src_root=$(CDPATH='' cd -- "$1" && pwd -P)
checker="$src_root/tests/_static/sh/staticcheck-accel-ledger.sh"
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-accel-controls-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
root=$tmpdir/root
mkdir -p "$root/src/sub" "$root/docs/acceleration" "$root/tests/_static/awk"
cp "$src_root/tests/_static/awk/accel-units.awk" \
    "$src_root/tests/_static/awk/accel-ledger.awk" "$root/tests/_static/awk/"

ledger="$root/docs/acceleration/ledger.md"
cat > "$ledger" <<'DATA'
<!-- accel-ledger simd -->

| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A-ONE | Tagged on several backends | done | done | n/a | done | planned | |
| A-SCALAR | Scalar lookup | done | n/a | n/a | n/a | n/a | |
| A-TWO | Only planned | done | planned | planned | planned | planned | |

<!-- accel-ledger-end -->

<!-- accel-ledger gpu -->

| ID | Operation | Scalar | Metal | Vulkan | WebGPU | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| A-ONE | Tagged on several backends | done | 8bit | done | 8bit | |

<!-- accel-ledger-end -->
DATA
cat > "$root/src/tagged.c" <<'DATA'
/* Accel: A-ONE neon */
static void
tagged(void)
{
    uint8x16_t v = vdupq_n_u8(0);
}

/* uint8x16_t in prose and an intrinsic or tag literal are ignored. */
static char const *prose = "_mm_add_epi8 /* Accel: ACCIDENTAL neon */";
static unsigned char value_u8 = 17;

/* Accel: A-ONE avx2 */
static void
wide(void)
{
    __m256i a = _mm256_setzero_si256();
    __m128i b = _mm_setzero_si128();
}

/* Accel: A-SCALAR scalar */
static const int table[] = {1, 2, 3};
DATA
printf '%s\n' '/* Accel: A-ONE vulkan vulkan-fp32 */' '#version 450' \
    'void main() {}' \
    > "$root/src/program.glsl"
printf '%s\n' '// Accel: A-ONE webgpu' '@compute @workgroup_size(1) fn main() {}' \
    > "$root/src/program.wgsl"
printf '%s\n' '/* Accel: A-ONE metal */' 'constant uint y = 1;' \
    > "$root/src/fragment.metal"
cat > "$root/src/tagged.m" <<'DATA'
/* Accel: A-ONE metal */
static char const *source = "kern"
    "el void f() {}";
DATA
printf '%s\n' 'static void s(void) { __m128i v; }' > "$root/src/stb_image.h"

failed=0
if sh "$checker" "$root" > "$tmpdir/actual"; then
    echo 'ok 1 - tagged SIMD, shader fragments and scalar code pass'
else
    echo 'not ok 1 - tagged SIMD, shader fragments and scalar code pass'
    sed 's/^/# /' "$tmpdir/actual"
    failed=1
fi

cat > "$ledger" <<'DATA'
<!-- accel-ledger simd -->
| ID | Operation | Scalar | NEON | SSE2 | AVX2 | WASM | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A-TWO | Only planned | done | planned | planned | planned | planned | |
| A-ONE | Tagged on several backends | done | done | n/a | done | done | |
| A-SCALAR | Scalar lookup | done | n/a | n/a | n/a | n/a | |
| B-BYTE | Precision state on a SIMD backend | done | 8bit | n/a | n/a | n/a | |
| C-BAD | Unknown state | done | maybe | n/a | n/a | n/a | |
| D-GPU | Listed in both tables without a SIMD backend | done | n/a | n/a | n/a | n/a | |
<!-- accel-ledger-end -->
<!-- accel-ledger gpu -->
| ID | Operation | Scalar | Metal | Vulkan | WebGPU | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| A-ONE | Tagged on several backends | done | done | 8bit | done | |
| D-GPU | Listed in both tables without a SIMD backend | planned | n/a | planned | n/a | |
| E-NONE | Listed without a GPU backend | done | n/a | n/a | n/a | |
| F-BYTE | A byte path without a tag | done | 8bit | n/a | n/a | |
<!-- accel-ledger-end -->
DATA
cat > "$root/src/drift.c" <<'DATA'
/* Accel: A-ONE neon */
static void mismatch(void) { __m128i v; }

/* Accel: A-ONE avx2 */
static void narrow(void) { __m128i v; }

/* Accel: A-ONE neon */
static void no_vector(void) { }

/* Accel: A-TWO neon */
static void planned(void) { uint8x16_t v; }

/* Accel: B-NONE neon */
static void unknown(void) { uint8x16_t v; }

/* Accel: bad neon */
static void malformed(void) { uint8x16_t v; }

static void misplaced(void)
{
    /* Accel: A-ONE neon */
    uint8x16_t v;
}
static void too_wide(void) { __m512i v; }
DATA
printf '%s\n' '#define NEON_ALIAS vreinterpretq_u8_s8' > "$root/src/sub/simd.inc"
printf '%s\n' 'kernel' 'void lone(device uint *x [[buffer(0)]]) {}' \
    > "$root/src/lone.metal"
printf '%s\n' 'constant uint x = 1;' > "$root/src/lone-fragment.metal"
printf '%s\n' '@compute @workgroup_size(1) fn main() {}' > "$root/src/lone.wgsl"
printf '%s\n' '// Accel: A-ONE webgpu-fp32' \
    '@compute @workgroup_size(1) fn main() {}' > "$root/src/program.wgsl"
printf '%s\n' '#version 450' 'void main() {}' > "$root/src/lone.comp"
cat > "$root/src/sub/embedded.c" <<'DATA'
static char const *source = "@comp"
    "ute @workgroup_size(1) fn main() {}";
DATA

status=0
sh "$checker" "$root" > "$tmpdir/actual" || status=$?
cat > "$tmpdir/expected" <<'DATA'
1..1
not ok 1 - acceleration units agree with the ledger
# ledger: A-ONE is out of order in the simd table
# ledger: A-ONE metal is done but no FP32 Accel tag names it
# ledger: A-ONE wasm is done but no Accel tag names it
# ledger: A-ONE webgpu is done but no Accel tag names it
# ledger: B-BYTE neon has unknown state "8bit"
# ledger: C-BAD neon has unknown state "maybe"
# ledger: D-GPU has no SIMD backend; remove its SIMD row
# ledger: D-GPU scalar differs between the tables
# ledger: E-NONE has no GPU backend; remove its GPU row
# ledger: F-BYTE metal is 8bit but no Accel tag names it
# src/drift.c:1 mismatch: Accel tag names unused backend neon
# src/drift.c:1 mismatch: uses sse2 without a matching Accel tag
# src/drift.c:12 unknown: Accel tag B-NONE is not in the ledger
# src/drift.c:15 malformed (neon): acceleration without an Accel tag
# src/drift.c:16: malformed Accel tag
# src/drift.c:18 misplaced (neon): acceleration without an Accel tag
# src/drift.c:21: place the Accel tag before the unit
# src/drift.c:24 too_wide (unsupported): acceleration without an Accel tag
# src/drift.c:24 too_wide: SIMD expression has no recognized backend
# src/drift.c:3 narrow: Accel tag names unused backend avx2
# src/drift.c:3 narrow: uses sse2 without a matching Accel tag
# src/drift.c:6 no_vector: Accel tag names unused backend neon
# src/drift.c:9 planned: Accel tag A-TWO neon but the ledger says planned
# src/lone-fragment.metal:1 lone-fragment.metal (metal): acceleration without an Accel tag
# src/lone.comp:1 lone.comp (vulkan): acceleration without an Accel tag
# src/lone.metal:1 lone (metal): acceleration without an Accel tag
# src/lone.wgsl:1 lone.wgsl (webgpu): acceleration without an Accel tag
# src/program.glsl:1 program.glsl: Accel tag A-ONE vulkan-fp32 but the ledger says 8bit
# src/sub/embedded.c:1 source (webgpu): acceleration without an Accel tag
# src/sub/simd.inc:1 NEON_ALIAS (neon): acceleration without an Accel tag
DATA
if test "$status" -eq 1 && cmp -s "$tmpdir/expected" "$tmpdir/actual"; then
    echo 'ok 2 - acceleration ledger drift fails the check'
else
    echo 'not ok 2 - acceleration ledger drift fails the check'
    diff "$tmpdir/expected" "$tmpdir/actual" | sed 's/^/# /' || :
    failed=1
fi
exit "$failed"
