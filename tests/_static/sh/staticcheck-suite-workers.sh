#!/bin/sh
# Verify ordered reporting and complete failure accounting with real workers.
# Policy: docs/testing/staticcheck.md
set -eux

echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-suite-workers-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/root/tests/_static/sh" "$tmpdir/root/tests/_static/awk"
cp "$src_root/tests/_static/sh/tap-case.sh" "$tmpdir/root/tests/_static/sh/"
cp "$src_root/tests/_static/awk/tap-status.awk" "$tmpdir/root/tests/_static/awk/"

# Exercise the production prelude and scheduler, replacing only registrations.
# A tiny source root prevents recursive execution of this meta-check.
awk '/^run_case_tap "staticcheck-/ { exit } { print }' \
    "$src_root/tests/_static/sh/staticcheck-suite.sh" > "$tmpdir/suite.sh"
cat >> "$tmpdir/suite.sh" <<'CASES'
run_case_tap "staticcheck-control-pass" sh -c 'printf "1..1\nok 1\n"'
run_case_tap "staticcheck-control-skip" sh -c 'printf "1..0 # SKIP absent\n"'
run_case_plain "staticcheck-control-plain" printf '%s\n' 'argument with spaces'
run_case_skip "staticcheck-control-absent" 'tool unavailable'
run_case_tap "staticcheck-control-bad-tap" sh -c 'printf "1..1\n"'
run_case_plain "staticcheck-control-child-failure" sh -c 'echo sentinel >&2; exit 9'
run_case_tap "staticcheck-control-last" sh -c 'printf "1..1\nok 1\n"'
CASES
awk '/^# Dispatch all registered cases/ { copy = 1 } copy' \
    "$src_root/tests/_static/sh/staticcheck-suite.sh" >> "$tmpdir/suite.sh"

for jobs in 1 3 000 invalid; do
    status=0
    STATICCHECK_JOBS=$jobs sh "$tmpdir/suite.sh" "$tmpdir/root" \
        > "$tmpdir/$jobs.log" 2>&1 || status=$?
    test "$status" -ne 0 || {
        echo 'not ok 1 - failed cases must fail the parallel suite'
        exit 0
    }
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
done
for jobs in 3 000 invalid; do
    cmp -s "$tmpdir/1.log" "$tmpdir/$jobs.log" || {
        echo 'not ok 1 - worker logs must retain serial registration order'
        exit 0
    }
done

# Infrastructure failure must not become a successful empty run.
mkdir "$tmpdir/bin"
printf '#!/bin/sh\nexit 1\n' > "$tmpdir/bin/xargs"
chmod +x "$tmpdir/bin/xargs"
status=0
PATH="$tmpdir/bin:$PATH" STATICCHECK_JOBS=3 \
    sh "$tmpdir/suite.sh" "$tmpdir/root" > "$tmpdir/missing.log" 2>&1 \
    || status=$?
output=$(cat "$tmpdir/missing.log")
case "$status:$output" in
    1:*'staticcheck summary: total=7 pass=0 skip=0 fail=7') ;;
    *)
        echo 'not ok 1 - missing worker results must fail closed'
        printf '%s\n' "$output"
        exit 0
        ;;
esac
# A host without xargs -P must still run every check serially.
CONTROL_XARGS=$(command -v xargs)
export CONTROL_XARGS
cat > "$tmpdir/bin/xargs" <<'SERIAL'
#!/bin/sh
for argument do
    test "$argument" != -P || exit 2
done
exec "$CONTROL_XARGS" "$@"
SERIAL
status=0
PATH="$tmpdir/bin:$PATH" STATICCHECK_JOBS=3 \
    sh "$tmpdir/suite.sh" "$tmpdir/root" > "$tmpdir/serial-host.log" 2>&1 \
    || status=$?
test "$status" -ne 0 || {
    echo 'not ok 1 - serial fallback must propagate failed checks'
    exit 0
}
cmp -s "$tmpdir/1.log" "$tmpdir/serial-host.log" || {
    echo 'not ok 1 - xargs without parallel support must preserve all checks'
    exit 0
}
echo 'ok 1 - parallel staticcheck preserves order and complete verdicts'
