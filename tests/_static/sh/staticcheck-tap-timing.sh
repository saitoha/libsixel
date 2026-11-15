#!/bin/sh
# Exercise TAP timing on a shell whose builtin time ignores TIMEFORMAT.
set -eux
echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tap-timing-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

# Emulate the OpenBSD timing output even on hosts whose sh is Bash or dash.
{
    cat <<'SH'
unset BASH_VERSION
bsd_time() {
    "$@"
    status=$?
    printf '%s\n' '0m0000s' >&2
    return "$status"
}
SH
    # shellcheck disable=SC2016
    sed -e 's/@OPENVMS_GNV_TRUE@/#/' \
        -e 's/$(command -v time)/time/' \
        -e 's/{ time run_and_capture/{ bsd_time run_and_capture/' \
        "$src_root/build-aux/lso-tap-driver.sh.in"
} > "$tmpdir/driver"
cat > "$tmpdir/timer" <<'SH'
#!/bin/sh
if test -f "$SIXEL_TAP_CLOCK_MARKER"; then
    printf '%s\n' 10.127
else
    printf '%s\n' 10.002
fi
SH
chmod +x "$tmpdir/timer"

for parser in yes no; do
    # The timestamp advances only after the child runs. A second run fails.
    # shellcheck disable=SC2016
    TOP_SRCDIR="$src_root" TOP_BUILDDIR="$tmpdir" SIXEL_TEST_SHELL=sh \
        SIXEL_RUNTIME='' LSO_TAP_TIMER_BIN="$tmpdir/timer" \
        SIXEL_TAP_CLOCK_MARKER="$tmpdir/child-$parser" reduce_fork=$parser \
        sh "$tmpdir/driver" --test-name clock --stream-output=no --timeout 0 \
        --log-file "$tmpdir/test.log" --trs-file "$tmpdir/test.trs" -- sh -c \
        'test ! -f "$SIXEL_TAP_CLOCK_MARKER" || exit 2
         : > "$SIXEL_TAP_CLOCK_MARKER"
         printf "1..1\nok 1 - elapsed\n"' >/dev/null || {
        echo 'not ok 1 - non-Bash TAP driver did not complete'
        exit 0
    }
    awk '
        /^:global-test-result: PASS$/ { result++ }
        /^:duration-ms: 125(\.000)?$/ { duration++ }
        END { exit result != 1 || duration != 1 }
    ' "$tmpdir/test.trs" || {
        echo 'not ok 1 - non-Bash TAP results lost the measured duration'
        sed 's/^/# /' "$tmpdir/test.trs"
        exit 0
    }
done
echo 'ok 1 - non-Bash TAP parses results with timestamp timing'
