#!/bin/sh
# Verify independent cache identities and conservative invalidation boundaries.
# Policy: docs/testing/staticcheck.md
set -eux

echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-shellcheck-cache-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/root with spaces/tests" "$tmpdir/home"
control_root="$tmpdir/root with spaces"
SHELLCHECK_CMD="$tmpdir/shellcheck"
CONTROL_REPORT="$tmpdir/checked"
TOP_BUILDDIR="$control_root"
TOP_SRCDIR="$control_root"
HOME="$tmpdir/home"
XDG_CONFIG_HOME="$HOME/.config"
APPDATA="$HOME/.config"
SHELLCHECK_OPTS=
STATICCHECK_CACHE=1
SIXEL_STATICCHECK_MAKE=
MAKEFLAGS=
export SHELLCHECK_CMD CONTROL_REPORT TOP_BUILDDIR TOP_SRCDIR HOME \
    XDG_CONFIG_HOME APPDATA SHELLCHECK_OPTS STATICCHECK_CACHE \
    SIXEL_STATICCHECK_MAKE MAKEFLAGS
cat > "$SHELLCHECK_CMD" <<'MOCK'
#!/bin/sh
set -eu
if test "${1:-}" = --version; then echo 'cache control 1'; exit 0; fi
test "$1" = -x && test "$2" = -P
shift 3
test "$#" -gt 0 && test "$#" -le 64
printf '%s\n' "$@" >> "$CONTROL_REPORT"
for path do
    while IFS= read -r line; do
        test "$line" != FAIL || exit 1
    done < "$path"
done
MOCK
chmod +x "$SHELLCHECK_CMD"
# Cross the batch boundary; no result is identified by a batch number.
awk -v root="$control_root/tests" 'BEGIN {
    for (i = 1; i <= 65; i++) {
        file = sprintf("%s/file-%03d.t", root, i)
        print "#!/bin/sh\necho ok" > file
        close(file)
    }
}'
touch -t 200001010000 "$control_root/tests/"*.t
for scenario in cold warm edit failed failed_again repaired added removed renamed \
    malformed tool_changed tool_warm rc_added rc_removed home_rc home_removed \
    options disabled force directories directories_warm directories_removed \
    unsafe_path unsafe_again unsafe_removed \
    source quoted_source quoted_dot escaped_source directive ansi_word empty; do
    expected=0
    expected_status=0
    SHELLCHECK_OPTS=
    STATICCHECK_CACHE=1
    MAKEFLAGS=
    case "$scenario" in
        cold) expected=65 ;;
        warm) ;;
        edit)
            printf '#!/bin/sh\necho changed\n' > "$control_root/tests/file-032.t"
            expected=1 ;;
        failed)
            printf '#!/bin/sh\nFAIL\n' > "$control_root/tests/file-032.t"
            expected=1; expected_status=1 ;;
        failed_again) expected=1; expected_status=1 ;;
        repaired)
            printf '#!/bin/sh\necho repaired\n' > "$control_root/tests/file-032.t"
            expected=1 ;;
        added)
            printf '#!/bin/sh\necho added\n' > "$control_root/tests/added.t"
            expected=1 ;;
        removed) rm "$control_root/tests/added.t" ;;
        renamed)
            mv "$control_root/tests/file-032.t" "$control_root/tests/renamed.t"
            expected=1 ;;
        malformed)
            find "$control_root/.staticcheck-cache" -name renamed.t.trs \
                -exec sh -c 'printf ":test-result: FAIL\n" > "$1"' sh {} \;
            expected=1 ;;
        tool_changed)
            printf '\n# Changed executable bytes.\n' >> "$SHELLCHECK_CMD"
            expected=65 ;;
        tool_warm) ;;
        rc_added)
            printf 'external-sources=true\n' > "$control_root/tests/.shellcheckrc"
            expected=65 ;;
        rc_removed) rm "$control_root/tests/.shellcheckrc" ;;
        home_rc)
            printf 'external-sources=true\n' > "$HOME/.shellcheckrc"
            expected=65 ;;
        home_removed) rm "$HOME/.shellcheckrc" ;;
        options) SHELLCHECK_OPTS='--exclude=SC2086'; expected=65 ;;
        disabled) STATICCHECK_CACHE=0; expected=65 ;;
        force) MAKEFLAGS=B; expected=65 ;;
        directories)
            mkdir "$control_root/tests/alpha" "$control_root/tests/beta"
            printf '#!/bin/sh\necho ok\n' > "$control_root/tests/alpha/shared.t"
            printf '#!/bin/sh\necho ok\n' > "$control_root/tests/beta/shared.t"
            touch -t 200001010000 "$control_root/tests/alpha/shared.t" \
                "$control_root/tests/beta/shared.t"
            expected=2 ;;
        directories_warm) ;;
        directories_removed)
            rm "$control_root/tests/alpha/shared.t" "$control_root/tests/beta/shared.t" ;;
        unsafe_path)
            printf '#!/bin/sh\necho ok\n' > "$control_root/tests/unsafe: file.t"
            expected=1 ;;
        unsafe_again) expected=1 ;;
        unsafe_removed) rm "$control_root/tests/unsafe: file.t" ;;
        source)
            printf '#!/bin/sh\n. ./helper.sh\n' > "$control_root/tests/renamed.t"
            expected=1 ;;
        quoted_source)
            printf '#!/bin/sh\n"so""urce" helper.sh\n' > "$control_root/tests/renamed.t"
            expected=1 ;;
        quoted_dot)
            printf '#!/bin/sh\n"." helper.sh\n' > "$control_root/tests/renamed.t"
            expected=1 ;;
        escaped_source)
            printf '#!/bin/sh\nso\\\nurce helper.sh\n' > "$control_root/tests/renamed.t"
            expected=1 ;;
        directive)
            printf '#!/bin/sh\n# shellcheck source=helper.sh\necho ok\n' > "$control_root/tests/renamed.t"
            expected=1 ;;
        ansi_word)
            printf '%s\n' '#!/bin/sh' "\$'\\x2e' helper.sh" > "$control_root/tests/renamed.t"
            expected=1 ;;
        empty) rm "$control_root/tests/"*.t ;;
    esac
    : > "$CONTROL_REPORT"
    status=0
    sh "$src_root/tests/_static/sh/shellcheck.sh" "$control_root" >/dev/null || status=$?
    test "$status" -eq "$expected_status" || {
        echo "not ok 1 - shellcheck cache status: $scenario"
        exit 0
    }
    awk -v expected="$expected" 'END { exit NR != expected }' "$CONTROL_REPORT" || {
        echo "not ok 1 - shellcheck cache pending count: $scenario"
        cat "$CONTROL_REPORT"
        exit 0
    }
    # Model stable, older inputs without sleeping for Make clock ticks. Failed
    # inputs retain their new time so the previous PASS cannot satisfy them.
    test "$status" -ne 0 || test "$scenario" = empty ||
        touch -t 200001010000 "$control_root/tests/"*.t
    # Dependency-bearing inputs must stay uncached even after a successful
    # invocation. A helper edit need not update the test file itself.
    case "$scenario" in
        source|quoted_source|quoted_dot|escaped_source|directive|ansi_word)
            : > "$CONTROL_REPORT"
            printf 'changed helper\n' > "$control_root/tests/helper.sh"
            sh "$src_root/tests/_static/sh/shellcheck.sh" "$control_root" >/dev/null || {
                echo "not ok 1 - shellcheck dependency status: $scenario"
                exit 0
            }
            awk 'END { exit NR != 1 }' "$CONTROL_REPORT" || {
                echo "not ok 1 - shellcheck dependencies must be rechecked: $scenario"
                exit 0
            }
            ;;
    esac
done
test -d "$control_root/.staticcheck-cache" &&
    test ! -e "$control_root/tests/.staticcheck-cache" || {
    echo 'not ok 1 - mutable cache artifacts must stay outside source scans'
    exit 0
}
echo 'ok 1 - shellcheck caches files independently and rechecks uncertain dependencies'
exit 0
