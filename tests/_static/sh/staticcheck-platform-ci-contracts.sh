#!/bin/sh
# Verify that documented platform evidence remains one exact CI matrix tuple.
# Policy: docs/misc/platforms/windows-paths.md
# Policy: docs/misc/platforms/haiku.md
# Policy: docs/misc/platforms/solaris.md
# Coverage: WPATH-01 HAIKU-02 SOL-02

set -eu

src_root=$1
contracts=$src_root/tests/_static/data/platform-ci-contracts.tsv
workflow=$src_root/.github/workflows/ci.yml
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-platform-ci-XXXXXX")
failed=0

trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

fail()
{
    echo "# $*" >&2
    failed=1
}

require_record_token()
{
    token=$1
    record=$2
    label=$3

    test "$token" = - && return
    grep -F -- "$token" "$record" >/dev/null 2>&1 ||
        fail "$label record is missing: $token"
}

echo "1..1"

test -f "$contracts" || fail "platform CI contract inventory is missing"
test -f "$workflow" || fail "public CI workflow is missing"

tab=$(printf '\t')
index=0
while IFS="$tab" read -r coverage label buildtool runner exec_type msystem \
        required1 required2; do
    case "$coverage" in
      ''|'#'*) continue ;;
    esac
    index=$((index + 1))
    record=$tmpdir/record-$index
    awk -v wanted="$label" '
    /^          - label: / {
        current=$0
        sub(/^          - label: /, "", current)
        if (found != 0) {
            exit
        }
        if (current == wanted) {
            found=1
            print
        }
        next
    }
    found != 0 {
        if ($0 ~ /^    steps:/ || $0 ~ /^  [A-Za-z0-9_-]+:/) {
            exit
        }
        print
    }
    ' "$workflow" > "$record"
    test -s "$record" || {
        fail "active CI record is missing: $label"
        continue
    }
    record_count=$(awk -v wanted="$label" '
    /^          - label: / {
        current=$0
        sub(/^          - label: /, "", current)
        if (current == wanted) {
            count++
        }
    }
    END { print count + 0 }
    ' "$workflow")
    test "$record_count" -eq 1 ||
        fail "$label must identify exactly one active CI record"
    require_record_token "buildtool: $buildtool" "$record" "$label"
    require_record_token "runner: $runner" "$record" "$label"
    require_record_token "exec_type: $exec_type" "$record" "$label"
    test "$msystem" = - ||
        require_record_token "msystem: $msystem" "$record" "$label"
    require_record_token "$required1" "$record" "$label"
    require_record_token "$required2" "$record" "$label"
done < "$contracts"

# These two incomplete combinations are policy gaps, not CI-backed support.
windows_emscripten=$(awk '
function inspect() {
    if (label ~ /emscripten/ && runner ~ /^windows/) {
        print label " -> " runner
    }
}
/^          - label: / {
    inspect()
    label=$0
    sub(/^          - label: /, "", label)
    runner=""
    next
}
/^            runner: / {
    runner=$0
    sub(/^            runner: /, "", runner)
}
END { inspect() }
' "$workflow")
test -z "$windows_emscripten" ||
    fail "Windows-hosted Emscripten requires a policy update: $windows_emscripten"

meson_msys_msvc=$(awk '
/^          - label: Meson-msys2/ && /msvc/ {
    label=$0
    sub(/^          - label: /, "", label)
    print label
}
' "$workflow")
test -z "$meson_msys_msvc" ||
    fail "Meson/MSYS2/MSVC requires a policy update: $meson_msys_msvc"

test "$failed" -eq 0 || {
    echo "not ok 1 - platform CI records match documented tuples"
    exit 1
}

echo "ok 1 - platform CI records match documented tuples"
exit 0
