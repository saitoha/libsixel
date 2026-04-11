#!/bin/sh
# Emit TAP for legacy -y scan option usage in shell tests.

set -eu

echo "1..1"

src_root=$1
tests_dir=$src_root/tests
allow_test="$tests_dir/cli/options/matching/0147_option_matching_diffusion_legacy_scan_option_rejected.t"

if test ! -d "$tests_dir"; then
    echo "ok 1 # SKIP missing tests directory"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-legacy-y-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

hits_all="$tmpdir/hits_all.txt"
hits_blocked="$tmpdir/hits_blocked.txt"
missing="$tmpdir/missing.txt"
status=0

if test ! -f "$allow_test"; then
    echo "# tests/cli/options/matching: missing 0147 legacy -y rejection test" \
        >> "$missing"
    status=1
fi

if test -f "$allow_test"; then
    if ! grep -E -n '(^|[[:space:]])-y([[:space:]]|$)' "$allow_test" \
            >/dev/null 2>&1; then
        echo "# tests/cli/options/matching: 0147 must exercise legacy -y rejection" \
            >> "$missing"
        status=1
    fi
fi

find "$tests_dir" -type f -name '*.t' -exec \
    grep -E -n -H '(^|[[:space:]])-y([[:space:]]|$)' {} + > "$hits_all" || true

if test -s "$hits_all"; then
    awk -v allow="$allow_test" -F: '
    {
        file = $1
        if (file != allow) {
            print
        }
    }
    ' "$hits_all" > "$hits_blocked"
fi

if test -s "$hits_blocked"; then
    echo "# tests: legacy -y scan option is forbidden outside 0147" >> "$missing"
    cat "$hits_blocked" >> "$missing"
    status=1
fi

if test "$status" -eq 0; then
    echo "ok 1 - legacy -y scan option stays confined to rejection test"
    exit 0
fi

echo "not ok 1 - legacy -y scan option stays confined to rejection test"
cat "$missing"
exit 1
