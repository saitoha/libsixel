#!/bin/sh
# Large inventories remain files through both CPU archive invocations.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v
src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-cosmoar-response-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
# Fake SDK tools inspect the production adapter without a compiler or SDK.
printf '#!/bin/sh\nexit 0\n' > "$fixture/cosmoar"
cat > "$fixture/check-ar" <<'EOF'
#!/bin/sh
set -eu
test "$#" = 3
test "$1" = csrD
case $0 in
    */x86_64-*) prefix=; expected=out.a ;;
    *) prefix=.aarch64/; expected=./.aarch64/out.a ;;
esac
test "$2" = "$expected"
case $3 in @*) ;; *) exit 1 ;; esac
awk -v prefix="$prefix" '
BEGIN { padding = sprintf("%0190d", 0) }
NR == 1 { if ($0 != "\"dir/" prefix "direct object.o\"") exit 1; next }
NR >= 2 && NR <= 2001 {
    if ($0 != "\"dir/" prefix padding (NR - 2) ".o\"") exit 1
    next
}
NR == 2002 { if ($0 != "\"dir/" prefix "quoted object.o\"") exit 1; next }
NR == 2003 { if ($0 != "\"dir/" prefix "a\\\"b\\\\c.o\"") exit 1; next }
{ exit 1 }
END { if (NR != 2003) exit 1 }
' "${3#@}"
printf '%s\n' "$2"
EOF
chmod +x "$fixture/cosmoar" "$fixture/check-ar"
ln -s check-ar "$fixture/x86_64-linux-cosmo-ar"
ln -s check-ar "$fixture/aarch64-linux-cosmo-ar"
awk 'BEGIN {
    padding = sprintf("%0190d", 0)
    for (i = 0; i < 2000; ++i) printf "dir/%s%d.o ", padding, i
    print "\047dir/quoted object.o\047 \"dir/a\\\"b\\\\c.o\""
}' > "$fixture/input.rsp"
cd "$fixture"
result=$(PATH="$fixture:$PATH" sh "$src_root/build-aux/cosmoar-meson" \
    csrD out.a 'dir/direct object.o' @input.rsp) || {
    echo 'not ok 1 - bounded paired archive inputs'
    exit 0
}
test "$result" = 'out.a
./.aarch64/out.a' || {
    echo 'not ok 1 - archive invocations or output paths changed'
    exit 0
}
echo 'ok 1 - bounded paired archive inputs'
exit 0
