#!/bin/sh
# Reject AVX2 kernels that take 256-bit or 512-bit vectors by value.
# Policy: docs/acceleration/README.md
#
# 32-bit x86 GCC and Cygwin/MSYS x86-64 GCC pass such an argument through
# a stack slot that is not 32-byte aligned and then load it with an aligned
# move, so the kernel faults only on those targets. Pass a pointer instead.
set -eu

echo '1..1'
src_root=$(CDPATH='' cd -- "$1" && pwd -P)
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-avx-params-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

# Collect each declarator that follows a *TARGET_AVX2 function attribute up
# to its opening brace or semicolon, drop qualifiers, and report a vector
# type followed directly by a parameter name.
cat > "$tmpdir/scan.awk" <<'EOF'
function flush() {
    text = collected
    gsub(/[ \t]+/, " ", text)
    gsub(/(const|volatile|restrict) ?/, "", text)
    sub(/^[^(]*\(/, "(", text)
    if (text ~ /[(,] ?__m(256|512)[a-z]* [A-Za-z_]/) {
        print FILENAME ":" start ": vector parameter passed by value"
        bad = 1
    }
    collecting = 0
    collected = ""
}
/^[ \t]*#/ { next }
!collecting && /TARGET_AVX2/ {
    collecting = 1
    start = FNR
    collected = ""
}
collecting {
    collected = collected " " $0
    if ($0 ~ /[{;]/) {
        flush()
    }
}
END { exit bad }
EOF

# The control must be rejected; the pointer form must be accepted.
cat > "$tmpdir/bad.c" <<'EOF'
static SIXEL_TARGET_AVX2 double
kernel(__m256d const sums, double population)
{
}
EOF
cat > "$tmpdir/good.c" <<'EOF'
static SIXEL_TARGET_AVX2 double
kernel(__m256d const *sums, __m256i *out, double population)
{
}
EOF
if awk -f "$tmpdir/scan.awk" "$tmpdir/bad.c" >/dev/null ||
        ! awk -f "$tmpdir/scan.awk" "$tmpdir/good.c" >/dev/null; then
    echo 'not ok 1 - AVX2 vector parameter scan controls'
    exit 0
fi

cd "$src_root"
sh "$source_files_helper" src --max-depth 0 --name '*.c' --name '*.h' \
    --exclude-name 'stb_image.h' --exclude-name 'stb_image_write.c' \
    --exclude-name 'stb_image_write.h' > "$tmpdir/files"
# shellcheck disable=SC2046
if ! awk -f "$tmpdir/scan.awk" $(cat "$tmpdir/files") > "$tmpdir/report"
then
    echo 'not ok 1 - AVX2 kernels take vectors by address'
    sed 's/^/# /' "$tmpdir/report"
    exit 0
fi
echo 'ok 1 - AVX2 kernels take vectors by address'
exit 0
