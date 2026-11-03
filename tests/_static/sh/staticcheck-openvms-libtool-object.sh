#!/bin/sh
# Resolve Libtool metadata before invoking the native OpenVMS linker.
# Policy: docs/misc/platforms/openvms.md
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Coverage: OV-24
set -eux
echo '1..1'
set -v
src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-vms-lo-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
mkdir -p "$fixture/objects/.libs"
: > "$fixture/objects/plain.o"
: > "$fixture/objects/.libs/pic.o"
cat > "$fixture/objects/plain.lo" <<'EOF'
# Libtool metadata must be read, never executed.
exit 42
pic_object='.libs/pic.o'
non_pic_object='plain.o'
EOF
cat > "$fixture/objects/pic.lo" <<'EOF'
pic_object='.libs/pic.o'
non_pic_object='none'
EOF
cat > "$fixture/dcl" <<'EOF'
#!/bin/sh
set -eu
# Inspect the actual option file before the wrapper cleans it up. No target
# compiler or native linker is needed to check this metadata boundary.
awk '
NR == 1 && /objects\]plain[.]o$/ { next }
NR == 2 && /objects[.]\^[.]libs\]pic[.]o$/ { next }
{ exit 1 }
END { if (NR != 2) exit 1 }
' "$TMPDIR"/libsixel-gnv-link-*.opt
: > "$TMPDIR/runner.exe"
EOF
chmod +x "$fixture/dcl"
TMPDIR="$fixture" GNV_DCL="$fixture/dcl" \
    sh "$src_root/openvms/gnv-link-program.sh" \
    -o "$fixture/runner.exe" "$fixture/objects/plain.lo" \
    "$fixture/objects/pic.lo" || {
    echo 'not ok 1 - OpenVMS resolves Libtool objects before native LINK'
    exit 0
}
echo 'ok 1 - OpenVMS resolves Libtool objects before native LINK'
exit 0
