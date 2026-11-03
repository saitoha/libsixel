#!/bin/sh
# Failed parsing or either CPU archiver cannot report a complete archive pair.
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v
src_root=$1
fixture=$(mktemp -d "${TMPDIR:-/tmp}/sixel-cosmoar-failure-XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM
printf '#!/bin/sh\nexit 0\n' > "$fixture/cosmoar"
cat > "$fixture/check-ar" <<'EOF'
#!/bin/sh
set -eu
printf 'partial\n' > "$2"
case $0 in *"$FAIL_CPU"*) exit 9 ;; esac
EOF
chmod +x "$fixture/cosmoar" "$fixture/check-ar"
ln -s check-ar "$fixture/x86_64-linux-cosmo-ar"
ln -s check-ar "$fixture/aarch64-linux-cosmo-ar"
cd "$fixture"
for FAIL_CPU in x86_64 aarch64; do
    export FAIL_CPU
    PATH="$fixture:$PATH" sh "$src_root/build-aux/cosmoar-meson" \
        csrD out.a object.o && {
        echo 'not ok 1 - CPU archive failure was hidden'
        exit 0
    }
    test ! -e out.a && test ! -e .aarch64/out.a || {
        echo 'not ok 1 - incomplete archive pair survived'
        exit 0
    }
done
printf 'original\n' > out.a
printf 'original\n' > .aarch64/out.a
printf '\042unterminated\n' > bad.rsp
for input in @missing.rsp @bad.rsp; do
    PATH="$fixture:$PATH" sh "$src_root/build-aux/cosmoar-meson" \
        csrD out.a "$input" && {
        echo 'not ok 1 - invalid response input was accepted'
        exit 0
    }
    test -s out.a || {
        echo 'not ok 1 - parsing failure removed an existing archive'
        exit 0
    }
    cmp -s out.a .aarch64/out.a || {
        echo 'not ok 1 - parsing failure modified existing archives'
        exit 0
    }
done
echo 'ok 1 - archive and response failures propagate'
exit 0
