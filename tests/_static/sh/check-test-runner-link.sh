#!/bin/sh
# Exercise the actual bounded link recipes without compiling C objects.
set -eu
src_root=$1
work=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-link-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
make_bin=${SIXEL_STATICCHECK_MAKE:-make}
command -v gmake >/dev/null 2>&1 && make_bin=${SIXEL_STATICCHECK_MAKE:-gmake}
run() (
    unset MAKEFLAGS MFLAGS MAKELEVEL
    "$make_bin" -s --no-print-directory -C "$work" "$@" test_runner
)
# Two actual files suffice for an argument list exceeding 128 KiB. Preserve
# their order and duplicates in both the manifest and the simulated link.
awk -v work="$work" -v root="$src_root" 'BEGIN {
    a = sprintf("%0160d", 0); gsub(/0/, "a", a)
    b = a; gsub(/a/, "b", b)
    a = "test_runner-" a ".o"; b = "test_runner-" b ".o"
    printf "" > (work "/" a); close(work "/" a)
    printf "" > (work "/" b); close(work "/" b)
    printf "test_runner_OBJECTS = "
    print "before" > (work "/expected")
    for (i = 0; i < 1025; i++) {
        printf "%s %s ", a, b
        print a > (work "/objects.expected"); print b > (work "/objects.expected")
        print a > (work "/expected"); print b > (work "/expected")
    }
    print "\ntest_runner_LINK = $(SHELL) verify.sh $@ before"
    print "test_runner_LDADD = library-after-objects\nLIBS = last-library"
    print "include " root "/build-aux/test-runner-link.am"
    print "library-after-objects\nlast-library" > (work "/expected")
}' > "$work/Makefile"
cat > "$work/verify.sh" <<'LINK'
#!/bin/sh
set -eu
test "${FAIL_LINK:-no}" != yes
output=$1
trap 'rm -f "$output.tmp"' EXIT HUP INT TERM
shift
{
    for arg do
        case "$arg" in
            @*) cat "${arg#@}" || exit 1 ;;
            *) printf '%s\n' "$arg" ;;
        esac
    done
} > "$output.tmp"
mv "$output.tmp" "$output"
LINK
run -n > "$work/dry"
test ! -e "$work/test_runner.objects"
awk 'length($0) >= 32768 { bad = 1 } END { exit bad }' "$work/dry"
run -j2 >/dev/null
cmp "$work/objects.expected" "$work/test_runner.objects"
cmp "$work/expected" "$work/test_runner"
run FAIL_LINK=yes >/dev/null
# A second dry run must contain neither manifest writes nor a linker call.
run -n > "$work/dry"
awk '/printf|verify\.sh/ { bad = 1 } END { exit bad }' "$work/dry"
rm "$work/test_runner"
run -n SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes > "$work/dry"
awk '/@test_runner.objects/ { found = 1 } /objects=\$\(cat/ { bad = 1 }
    END { exit (bad || !found) }' "$work/dry"
run SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes >/dev/null
cmp "$work/expected" "$work/test_runner"
run FAIL_LINK=yes SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes >/dev/null
run -n SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes > "$work/dry"
awk '/printf|verify\.sh/ { bad = 1 } END { exit bad }' "$work/dry"
# Change only the configured inventory and force the existing outputs older.
sed '1s/ = [^ ]* / = /' "$work/Makefile" > "$work/Makefile.new"
mv "$work/Makefile.new" "$work/Makefile"
touch -t 200001010000 "$work/test_runner.objects" "$work/test_runner"
run >/dev/null
sed '1d' "$work/objects.expected" > "$work/objects.changed"
cmp "$work/objects.changed" "$work/test_runner.objects"
rm "$work/test_runner.objects" "$work/test_runner"
mkdir "$work/test_runner.objects"
for mode in no yes; do
    status=0
    run "SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=$mode" >/dev/null 2>&1 || status=$?
    test "$status" -ne 0 && test ! -e "$work/test_runner"
done
# Run the OpenVMS config.status repair verbatim, with a lost dependency list.
: > "$work/runner.o"
{
    awk '/^EXTRA_LTLIBRARIES = libfloatlookuptests.la$/ { print; found = 1 }
        END { if (!found) exit 1 }' "$src_root/tests/Makefile.am"
    cat <<'MAKE'
am_test_runner_OBJECTS = runner.o
libfloatlookuptests_la_OBJECTS = first-lookup.lo second-lookup.lo
test_runner_DEPENDENCIES =
test_runner:
	@test "$(FAIL_LINK)" != yes
	@for object in $(libfloatlookuptests_la_OBJECTS); do test -f $$object || exit 1; done
	@touch $@
$(libfloatlookuptests_la_OBJECTS):
	@test "$(FAIL_PREREQUISITE)" != yes
	@touch $@
MAKE
    awk '/^SIXEL_OPENVMS_TEST_OBJECTS$/ { active = 0 }
        active { print }
        /<<'\''SIXEL_OPENVMS_TEST_OBJECTS'\''/ { active = 1 }
    ' "$src_root/configure.ac"
} > "$work/Makefile"
# The OpenVMS repair replaces the ordinary manifest prerequisite entirely.
run -n >/dev/null
test ! -e "$work/test_runner"
status=0
run FAIL_PREREQUISITE=yes >/dev/null 2>&1 || status=$?
test "$status" -ne 0 && test ! -e "$work/test_runner"
run >/dev/null
run FAIL_LINK=yes >/dev/null
touch -t 200001010000 "$work/runner.o" "$work/test_runner"
status=0
run FAIL_LINK=yes >/dev/null 2>&1 || status=$?
test "$status" -ne 0
