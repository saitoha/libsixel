#!/bin/sh
# A package resolution failure must refresh the indexes before retrying.

set -eux
echo '1..1'
set -v

installer=$1/.github/scripts/install-ubuntu-packages.sh
{ test -f "$installer" && command -v bash >/dev/null; } || {
    echo 'ok 1 # SKIP CI package installer or Bash unavailable'
    exit 0
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-ci-apt.XXXXXX") || {
    echo 'not ok 1 - cannot prepare an isolated apt mock'
    exit 0
}
trap 'rm -rf "$tmpdir"' EXIT

cat >"$tmpdir/sudo" <<'MOCK'
#!/bin/sh
set -eu
test "$1" != grep || exit 1
test "$1" = timeout && test "$4" = apt-get
shift 4
while test "$1" = -o; do
    test "$2" != APT::Update::Error-Mode=any || strict=yes
    shift 2
done
case "$1" in
    update)
        test "${strict:-}" = yes
        printf 'update\n' >> "$APT_TEST_LOG"
        ;;
    install)
        printf 'install\n' >> "$APT_TEST_LOG"
        test "$(grep -cx update "$APT_TEST_LOG")" -ge 2 || exit 100
        ;;
    *) exit 1 ;;
esac
MOCK
printf '#!/bin/sh\nexit 0\n' >"$tmpdir/sleep"
chmod +x "$tmpdir/sudo" "$tmpdir/sleep"

PATH="$tmpdir:$PATH" APT_TEST_LOG="$tmpdir/apt.log" bash "$installer" meson || {
    echo 'not ok 1 - dependency resolution did not recover'
    exit 0
}
test "$(cat "$tmpdir/apt.log")" = "$(printf 'update\ninstall\nupdate\ninstall')" || {
    cat "$tmpdir/apt.log"
    echo 'not ok 1 - each install retry needs a fresh index update'
    exit 0
}
echo 'ok 1 - apt install retries refresh package indexes'
exit 0
