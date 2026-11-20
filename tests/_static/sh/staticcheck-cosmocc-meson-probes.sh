#!/bin/sh
# The Meson compiler adapter must execute the linked APE probes.
set -eux
echo '1..1'
set -v

adapter=$1/build-aux/cosmocc-meson
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-cosmo-probe.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT

cat >"$tmpdir/cosmocc" <<'MOCK'
#!/bin/sh
set -eu
test "$1" = -o
cat >"$2" <<'APE'
MZqFpD='
'
test "$1" = expected-argument || exit 8
exit 7
APE
chmod +x "$2"
MOCK
chmod +x "$tmpdir/cosmocc"
export PATH="$tmpdir:$PATH"
mkdir -p "$tmpdir/meson-private/probe"
cd "$tmpdir/meson-private"

for probe in sanity_check_for_c.exe sanitycheckc.exe "$PWD/probe/output.exe"; do
    sh "$adapter" -o "$probe" || {
        echo 'not ok 1 - APE probe was not linked'
        exit 0
    }
    test "$(head -n 1 "$probe")" = '#!/bin/sh' || {
        echo 'not ok 1 - Meson needs a launcher for APE probes'
        exit 0
    }
    status=0
    case "$probe" in
        /*) "$probe" expected-argument || status=$? ;;
        *) "./$probe" expected-argument || status=$? ;;
    esac
    test "$status" -eq 7 || {
        echo 'not ok 1 - launcher must preserve the probe arguments and status'
        exit 0
    }
done
cd "$tmpdir"
sh "$adapter" -o program.exe
test "$(head -n 1 program.exe)" = "MZqFpD='" || {
    echo 'not ok 1 - ordinary compiler outputs must remain APE binaries'
    exit 0
}
echo 'ok 1 - Meson executes real Cosmopolitan probes'
exit 0
