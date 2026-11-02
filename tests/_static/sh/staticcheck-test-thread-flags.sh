#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/build/test-harness.md
set -eux
echo '1..1'
set -v
src_root=$1
checker=$src_root/tests/_static/awk/test-thread-flags.awk
awk -f "$checker" "$src_root/tests/Makefile.am" >&2 || {
    echo 'not ok 1 - test support thread ABI flags'
    exit 0
}
sed 's/@THREAD_CFLAGS@//g' "$src_root/tests/Makefile.am" |
    awk -f "$checker" >&2 && {
    echo 'not ok 1 - missing thread ABI flags escaped detection'
    exit 0
}
echo 'ok 1 - test support thread ABI flags'
exit 0
