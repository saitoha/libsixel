#!/bin/sh
# Both parsers must retain actual TAP and child-process failures.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tap-status-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
sed 's/@OPENVMS_GNV_TRUE@/#/' "$src_root/build-aux/lso-tap-driver.sh.in" > "$tmpdir/driver"
for parser in no yes; do
    for child in 'printf "1..1\nnot ok 1 - assertion\n"' \
        'printf "1..1\nok 1 - stdout\n"; exit 2'; do
        TOP_SRCDIR="$src_root" TOP_BUILDDIR="$tmpdir" \
            SIXEL_TEST_SHELL=sh reduce_fork=$parser \
            sh "$tmpdir/driver" --test-name failure \
            --log-file "$tmpdir/test.log" --trs-file "$tmpdir/test.trs" \
            -- sh -c "$child" >/dev/null || {
            echo 'not ok 1 - normal driver did not finish result collection'
            exit 0
        }
        awk '/^:global-test-result: FAIL$/ { good=1 }
            END { exit !good }' "$tmpdir/test.trs" || {
            echo 'not ok 1 - actual test failure was lost'
            exit 0
        }
        status=0
        TOP_SRCDIR="$src_root" TOP_BUILDDIR="$tmpdir" \
            SIXEL_TEST_SHELL=sh reduce_fork=$parser \
            sh "$tmpdir/driver" --test-name failure --enable-hard-errors=yes \
            --log-file "$tmpdir/test.log" --trs-file "$tmpdir/test.trs" \
            -- sh -c "$child" >/dev/null || status=$?
        test "$status" -eq 1 || {
            echo 'not ok 1 - hard-error mode lost the failure exit status'
            exit 0
        }
    done
done
echo 'ok 1 - TAP and process failures survive both parsers'
exit 0
