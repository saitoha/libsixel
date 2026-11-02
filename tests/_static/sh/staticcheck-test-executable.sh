#!/bin/sh
# Policy: docs/testing/guide.md
# Check filesystem modes and, on mode-blind checkouts, authoritative Git modes.
set -eux
echo '1..1'
set -v
src_root=$(CDPATH='' cd -- "$1" && pwd -P)
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-test-modes-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
filesystem_modes=1
case "$(uname -s)" in MINGW*|MSYS*) filesystem_modes=0 ;; esac
repository=0
top=$(git -C "$src_root" rev-parse --show-toplevel 2>/dev/null) &&
    test "$top" = "$src_root" && repository=1
index_modes=0
if test "$repository" = 1; then
    setting=$(git -C "$src_root" config --bool core.filemode) || setting=
    test "$filesystem_modes" = 1 && test "$setting" != false || index_modes=1
fi
test "$filesystem_modes" = 1 || test "$repository" = 1 || {
    echo 'ok 1 # SKIP executable metadata unavailable without Git on this platform'
    exit 0
}
sh "$src_root/tests/_static/sh/list-source-files.sh" "$src_root/tests" \
    --name '*.t' --exclude-path '*/data/*' --include-links \
    --exec sh "$src_root/tests/_static/sh/check-test-file-modes.sh" \
    > "$work/modes" || {
    echo 'not ok 1 - cannot inspect shell TAP filesystem modes'
    exit 0
}
awk -v filesystem_modes="$filesystem_modes" '
    substr($0, 1, 1) != "-" || (filesystem_modes && substr($0, 4, 1) !~ /[xs]/) {
        print "# expected regular shell TAP with owner execute bit: " $0
        failed = 1
    }
    END { exit (failed || NR == 0) }
' "$work/modes" || {
    echo 'not ok 1 - shell TAP filesystem modes are not executable'
    exit 0
}
if test "$index_modes" = 1; then
    # Git reports staged paths that may already be deleted in the worktree.
    # Audit only files present in this snapshot, as the filesystem pass does.
    git -C "$src_root" ls-files --stage -- tests/ > "$work/index" || {
        echo 'not ok 1 - cannot inspect shell TAP Git modes'
        exit 0
    }
    git -C "$src_root" ls-files --deleted -- tests/ > "$work/deleted" || {
        echo 'not ok 1 - cannot inspect deleted shell TAP paths'
        exit 0
    }
    git -C "$src_root" ls-files --others --exclude-standard -- tests/ > "$work/untracked" || {
        echo 'not ok 1 - cannot inspect shell TAP Git modes'
        exit 0
    }
    awk '
        FILENAME ~ /\/deleted$/ { deleted[$0] = 1; next }
        FILENAME ~ /\/index$/ { path = $0; sub(/^[^\t]*\t/, "", path); if (path in deleted) next }
        FILENAME ~ /\/index$/ && /\.t$/ && ($1 != "100755" || $3 != "0") {
            print "# expected Git mode 100755: " $0; failed = 1
        }
        FILENAME ~ /\/untracked$/ && /\.t$/ {
            print "# shell TAP needs an executable Git index entry: " $0; failed = 1
        }
        END { exit failed }
    ' "$work/deleted" "$work/index" "$work/untracked" || {
        echo 'not ok 1 - shell TAP Git modes are not executable'
        exit 0
    }
fi
echo 'ok 1 - shell TAP sources are executable'
exit 0
