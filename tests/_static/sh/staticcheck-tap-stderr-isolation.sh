#!/bin/sh
# TAP classification must ignore result-shaped stderr in either parser.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tap-isolation-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
sed 's/@OPENVMS_GNV_TRUE@/#/' "$src_root/build-aux/lso-tap-driver.sh.in" > "$tmpdir/driver"
for parser in no yes; do
    for stream in no yes; do
        # Keep all diagnostics, but only stdout can report an observation.
        # shellcheck disable=SC2016
        TOP_SRCDIR="$src_root" TOP_BUILDDIR="$tmpdir" \
            SIXEL_TEST_SHELL=sh reduce_fork=$parser \
            sh "$tmpdir/driver" --test-name isolation \
            --stream-output="$stream" --log-file "$tmpdir/test.log" \
            --trs-file "$tmpdir/test.trs" -- sh -c \
            'printf "not ok 99 - stderr\n1..100\n" >&2; printf "1..1\nok 1 - stdout\n"' \
            >/dev/null || {
            echo 'not ok 1 - diagnostic isolation driver failed'
            exit 0
        }
        awk '/^:test-result:/ { n++; if ($2 != "PASS") bad=1 }
            /^:global-test-result: PASS$/ { good=1 }
            /^not ok 99 - stderr$/ { err=1 }
            /^ok 1 - stdout$/ { out=1 }
            END { exit bad || n != 1 || !good || !err || !out }' \
            "$tmpdir/test.trs" "$tmpdir/test.log" || {
            echo 'not ok 1 - stderr changed TAP results or was discarded'
            exit 0
        }
        TOP_SRCDIR="$src_root" TOP_BUILDDIR="$tmpdir" \
            SIXEL_TEST_SHELL=sh reduce_fork=$parser \
            sh "$tmpdir/driver" --test-name stderr-only \
            --stream-output="$stream" --log-file "$tmpdir/test.log" \
            --trs-file "$tmpdir/test.trs" -- sh -c \
            'printf "1..1\nok 1 - stderr\n" >&2' >/dev/null || {
            echo 'not ok 1 - diagnostic-only driver failed'
            exit 0
        }
        awk '/^:global-test-result: ERROR$/ { good=1 }
            END { exit !good }' "$tmpdir/test.trs" || {
            echo 'not ok 1 - stderr alone supplied a passing observation'
            exit 0
        }
    done
done
echo 'ok 1 - TAP results and stderr diagnostics remain separate'
exit 0
