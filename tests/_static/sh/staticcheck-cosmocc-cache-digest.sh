#!/bin/sh
# Exercise the production cache verification without files or downloads.
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/cosmopolitan.md
# Coverage: COSMO-07
set -eux
command -v bash >/dev/null 2>&1 || {
    echo '1..0 # SKIP Bash is unavailable for the CI recipe'
    exit 0
}
echo '1..1'
set -v
recipe=$(awk '
/^    - name: Prepare Cosmopolitan toolchain$/ { selected = 1; next }
selected && /^    - name:/ { exit }
selected && /^      run: \|/ { body = 1; next }
selected && body { sub(/^        /, ""); print }
' "$1/.github/actions/ci-steps/action.yml")
test -n "$recipe" || {
    echo 'not ok 1 - Cosmopolitan cache verification recipe is present'
    exit 0
}
# Mock the short-option-only BSD interface and the existing shasum fallback.
# Pipeline readers verify the supplied digest and exact archive operand.
# shellcheck disable=SC2016
script='COSMOCC_VERSION=4.0.2
COSMOCC_SHA256=expected-digest
downloaded=0
published=0
unpacked=0
command() {
    if test "$*" = "-v sha256sum"; then
        test "$HASH_TOOL" = sha256sum
    else
        builtin command "$@"
    fi
}
sha256sum() {
    test "$HASH_TOOL" = sha256sum || exit 91
    verify_digest "$@"
}
shasum() {
    test "$HASH_TOOL" = shasum || exit 91
    test "$1 $2" = "-a 256" || exit 91
    shift 2
    verify_digest "$@"
}
verify_digest() {
    test "$*" = "-c -" || exit 91
    IFS= read -r record || exit 91
    case "$record" in
        "expected-digest  cosmopolitan/cosmocc-4.0.2.zip")
            return "$CACHE_STATUS" ;;
        "expected-digest  cosmopolitan/cosmocc-4.0.2.zip.part")
            return "$DOWNLOAD_DIGEST_STATUS" ;;
        *) exit 91 ;;
    esac
}
mkdir() { test "$*" = "-p cosmopolitan" || exit 91; }
curl() {
    test "$CACHE_STATUS" -ne 0 || exit 91
    test "$downloaded" -eq 0 || exit 91
    downloaded=1
    return "$DOWNLOAD_STATUS"
}
mv() {
    test "$downloaded" -eq 1 || exit 91
    test "$DOWNLOAD_DIGEST_STATUS" -eq 0 || exit 91
    test "$*" = "cosmopolitan/cosmocc-4.0.2.zip.part cosmopolitan/cosmocc-4.0.2.zip" || exit 91
    published=1
}
unzip() {
    test "$*" = "-o -q cosmopolitan/cosmocc-4.0.2.zip" || exit 91
    test "$CACHE_STATUS" -eq 0 || test "$published" -eq 1 || exit 91
    unpacked=1
}
chmod() { test "$unpacked" -eq 1 || exit 91; }
'
script="$script
$recipe
test \"\$unpacked\" -eq 1"
for HASH_TOOL in sha256sum shasum; do
    export HASH_TOOL
    for CACHE_STATUS in 0 1; do
        export CACHE_STATUS
        DOWNLOAD_STATUS=0 DOWNLOAD_DIGEST_STATUS=0 bash -euc "$script" || {
            echo 'not ok 1 - Cosmopolitan validates cached and downloaded bytes'
            exit 0
        }
    done
    result=0
    CACHE_STATUS=1 DOWNLOAD_STATUS=0 DOWNLOAD_DIGEST_STATUS=17 \
        bash -euc "$script" || result=$?
    test "$result" -eq 17 || {
        echo 'not ok 1 - Cosmopolitan rejects a corrupt downloaded archive'
        exit 0
    }
    result=0
    CACHE_STATUS=1 DOWNLOAD_STATUS=56 DOWNLOAD_DIGEST_STATUS=0 \
        bash -euc "$script" || result=$?
    test "$result" -eq 56 || {
        echo 'not ok 1 - Cosmopolitan propagates download failure'
        exit 0
    }
done
echo 'ok 1 - Cosmopolitan verifies archives through portable checksum options'
exit 0
