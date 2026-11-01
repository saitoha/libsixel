#!/bin/sh
# Verify ordered reporting and complete failure accounting with real workers.
# Policy: docs/testing/staticcheck.md
set -eux

echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-suite-workers-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/root/tests/_static/sh" "$tmpdir/root/tests/_static/awk" "$tmpdir/root/tests/_static/python"
cp "$src_root/tests/_static/sh/run-check-batches.sh" "$tmpdir/root/tests/_static/sh/"
cp "$src_root/tests/_static/sh/collect-source-references.sh" "$tmpdir/root/tests/_static/sh/"
cp "$src_root/tests/_static/awk/source-references.awk" "$tmpdir/root/tests/_static/awk/"
cp "$src_root/tests/_static/sh/list-source-files.sh" "$tmpdir/root/tests/_static/sh/"
cp "$src_root/tests/_static/python/source_files.py" "$tmpdir/root/tests/_static/python/"
cp "$src_root/tests/_static/sh/tap-case.sh" "$tmpdir/root/tests/_static/sh/"
cp "$src_root/tests/_static/awk/tap-status.awk" "$tmpdir/root/tests/_static/awk/"

# Exercise the production prelude and scheduler, replacing only registrations.
# A tiny source root prevents recursive execution of this meta-check.
awk '/^run_case_tap "staticcheck-/ { exit } { print }' \
    "$src_root/tests/_static/sh/staticcheck-suite.sh" > "$tmpdir/suite.sh"
cat >> "$tmpdir/suite.sh" <<'CASES'
run_case_tap "staticcheck-control-pass" sh -c 'printf "1..1\nok 1\n"'
run_case_tap "staticcheck-control-skip" sh -c 'printf "1..0 # SKIP absent\n"'
run_case_plain "staticcheck-codespell" printf '%s\n' 'argument with spaces' "quote'quote" '$literal' '' 'two
lines'
run_case_skip "staticcheck-control-absent" 'tool unavailable'
run_case_tap "staticcheck-control-bad-tap" sh -c 'printf "1..1\n"'
run_case_plain "staticcheck-control-child-failure" sh -c 'echo sentinel >&2; exit 9'
run_case_tap "staticcheck-control-last" sh -c '
    case "$TMPDIR" in "$CONTROL_ROOT/root/.staticcheck-tmp/"*) ;; *) exit 9 ;; esac
    # The start event must already be visible while this check is running.
    awk '\''$0 == "[staticcheck] START staticcheck-control-last" { found=1 }
        END { exit !found }'\'' "$CONTROL_ROOT/progress.log" || exit 9
    # A background shell inherits SIGINT ignored and cannot restore it.
    # Self-signalling tests that contract without interrupting the test runner.
    trap '\''printf "1..1\nok 1\n"; exit 0'\'' INT
    kill -INT $$
    exit 9
'
CASES
awk '/^# Dispatch all registered cases/ { copy = 1 } copy' \
    "$src_root/tests/_static/sh/staticcheck-suite.sh" >> "$tmpdir/suite.sh"

# Launch real recursive Make jobs rather than parsing or simulating MAKEFLAGS.
# Reset the outer jobserver only for these small isolated scheduling controls.
CONTROL_ROOT=$tmpdir
# The retired override must never supersede the caller's -j setting.
STATICCHECK_JOBS=99
export CONTROL_ROOT STATICCHECK_JOBS
cat > "$tmpdir/Makefile" <<'MAKEFILE'
all:
	+@SIXEL_STATICCHECK_MAKE='$(MAKE)' sh "$$CONTROL_ROOT/suite.sh" "$$CONTROL_ROOT/root" > "$$CONTROL_ROOT/result.log" 2> "$$CONTROL_ROOT/progress.log"
MAKEFILE
for jobs in default 1 3; do
    status=0
    set --
    test "$jobs" = default || set -- -j "$jobs"
    MAKEFLAGS='' MFLAGS='' MAKELEVEL=0 make --no-print-directory -s "$@" \
        -f "$tmpdir/Makefile" > "$tmpdir/make.log" 2>&1 || status=$?
    test "$status" -ne 0 || {
        echo 'not ok 1 - failed cases must fail the parallel suite'
        exit 0
    }
    cp "$tmpdir/result.log" "$tmpdir/$jobs.log"
    output=$(cat "$tmpdir/$jobs.log")
    case "$output" in
        *'argument with spaces'*'sentinel'*'PASS staticcheck-control-last'*\
'staticcheck summary: total=7 pass=3 skip=2 fail=2') ;;
        *)
            echo 'not ok 1 - worker result or failure diagnostics were lost'
            printf '%s\n' "$output"
            exit 0
            ;;
    esac
    set -- "$tmpdir/root/.staticcheck-tmp/"*
    test ! -e "$1" || {
        echo 'not ok 1 - completed suites must remove their local workspace'
        exit 0
    }
done
cmp -s "$tmpdir/1.log" "$tmpdir/3.log" || {
    echo 'not ok 1 - worker logs must retain serial registration order'
    exit 0
}

# A direct invocation has no Make-owned slots and must still check every case.
status=0
SIXEL_STATICCHECK_MAKE='' sh "$tmpdir/suite.sh" "$tmpdir/root" \
    > "$tmpdir/direct.log" 2> "$tmpdir/progress.log" || status=$?
# Both the failed status and identical output are required.
# shellcheck disable=SC2015
test "$status" -ne 0 && cmp -s "$tmpdir/1.log" "$tmpdir/direct.log" || {
    echo 'not ok 1 - direct serial execution must preserve all checks'
    exit 0
}

# Infrastructure failure must not become a successful empty run.
status=0
SIXEL_STATICCHECK_MAKE=false sh "$tmpdir/suite.sh" "$tmpdir/root" \
    > "$tmpdir/missing.log" 2>&1 || status=$?
output=$(cat "$tmpdir/missing.log")
case "$status:$output" in
    1:*'staticcheck summary: total=7 pass=0 skip=0 fail=7') ;;
    *)
        echo 'not ok 1 - missing worker results must fail closed'
        printf '%s\n' "$output"
        exit 0
        ;;
esac

# Two sibling schedulers each launch nested batches, as suite/lint workers do.
# Their leaf jobs must share the caller's slots, not multiply its -j budget.
printf 'one\000two\000three\000four\000' > "$tmpdir/queue"
cat > "$tmpdir/leaf.sh" <<'LEAF'
#!/bin/sh
# Successful signal handling is required before this worker records any work.
interrupted=no
trap 'interrupted=yes' INT
kill -INT $$
test "$interrupted" = yes || exit 9
printf 'start\n' >> "$CONTROL_ROOT/events"
sleep 0.1
printf 'end\n' >> "$CONTROL_ROOT/events"
LEAF
cat > "$tmpdir/nested.sh" <<'NESTED'
#!/bin/sh
exec sh "$CONTROL_ROOT/root/tests/_static/sh/run-check-batches.sh" \
    1 "$CONTROL_ROOT/queue" sh "$CONTROL_ROOT/leaf.sh"
NESTED
cat > "$tmpdir/Makefile" <<'MAKEFILE'
all: first second
first second:
	+@SIXEL_STATICCHECK_MAKE='$(MAKE)' sh "$$CONTROL_ROOT/nested.sh"
MAKEFILE
for jobs in default 1 3; do
    : > "$tmpdir/events"
    set --
    test "$jobs" = default || set -- -j "$jobs"
    MAKEFLAGS='' MFLAGS='' MAKELEVEL=0 make --no-print-directory -s "$@" \
        -f "$tmpdir/Makefile" > "$tmpdir/make.log" 2>&1 || {
        echo 'not ok 1 - nested batch scheduler failed'
        cat "$tmpdir/make.log"
        exit 0
    }
    limit=$jobs
    test "$limit" != default || limit=1
    awk -v limit="$limit" '
        $0 == "start" { active++; count++; if (active > peak) peak=active }
        $0 == "end" { active-- }
        active < 0 || active > limit { bad=1 }
        END { exit (bad || active != 0 || count != 8 || peak != limit) }
    ' "$tmpdir/events" || {
        echo 'not ok 1 - nested workers must share the Make job budget'
        cat "$tmpdir/events"
        exit 0
    }
    test ! -s "$tmpdir/make.log" || {
        echo 'not ok 1 - nested Make lost its jobserver'
        cat "$tmpdir/make.log"
        exit 0
    }
done
# Recursive recipes also run under -n, but their checks must not execute.
: > "$tmpdir/events"
MAKEFLAGS='' MFLAGS='' MAKELEVEL=0 make --no-print-directory -sn -j3 \
    -f "$tmpdir/Makefile" > "$tmpdir/make.log" 2>&1 || {
    echo 'not ok 1 - recursive dry run must succeed'
    exit 0
}
test ! -s "$tmpdir/events" || {
    echo 'not ok 1 - recursive dry run executed checks'
    exit 0
}
# Exercise the real codespell pipeline with a tool that requires SIGINT.
mkdir -p "$tmpdir/root/src"
printf 'int example;\n' > "$tmpdir/root/src/example.c"
cat > "$tmpdir/codespell" <<'TOOL'
#!/bin/sh
interrupted=no
trap 'interrupted=yes' INT
kill -INT $$
test "$interrupted" = yes
TOOL
chmod +x "$tmpdir/codespell"
SIXEL_STATICCHECK_MAKE='' sh "$src_root/tests/_static/sh/staticcheck-codespell.sh" \
    "$tmpdir/root" "$tmpdir/codespell" > "$tmpdir/codespell.log" 2>&1 || {
    echo 'not ok 1 - codespell workers must retain SIGINT handling'
    cat "$tmpdir/codespell.log"
    exit 0
}
echo 'ok 1 - staticcheck shares Make slots and preserves complete verdicts'
