#!/bin/sh
# Unavailable interpreters produce a skip through either TAP parser.
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tap-skip-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
sed 's/@OPENVMS_GNV_TRUE@/#/' "$src_root/build-aux/lso-tap-driver.sh.in" > "$tmpdir/driver"
for parser in no yes; do
    for interpreter in '' "$tmpdir/unavailable"; do
        for extension in py rb pl php; do
            TOP_SRCDIR="$src_root" TOP_BUILDDIR="$tmpdir" \
                SIXEL_TEST_PYTHON=$interpreter \
                SIXEL_TEST_RUBY=$interpreter \
                SIXEL_TEST_PERL=$interpreter \
                SIXEL_TEST_PHP=$interpreter reduce_fork=$parser \
                sh "$tmpdir/driver" --test-name unavailable \
                --log-file "$tmpdir/test.log" \
                --trs-file "$tmpdir/test.trs" \
                -- "$tmpdir/test.$extension" >/dev/null || {
                echo 'not ok 1 - unavailable interpreter driver failed'
                exit 0
            }
            awk '/^:test-result:/ { n++; if ($2 != "SKIP") bad=1 }
                /^:global-test-result: SKIP$/ { good=1 }
                /^1\.\.0 # SKIP .* interpreter is unavailable$/ { plan=1 }
                END { exit bad || n != 1 || !good || !plan }' \
                "$tmpdir/test.trs" "$tmpdir/test.log" || {
                echo 'not ok 1 - unavailable interpreter lost its TAP skip'
                exit 0
            }
        done
    done
done
echo 'ok 1 - unavailable interpreters remain skipped in both TAP parsers'
exit 0
