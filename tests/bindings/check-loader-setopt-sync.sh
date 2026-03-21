#!/bin/sh
# Verify that loader_setopt coverage stays aligned across language bindings.

set -eu

top_srcdir=${1:-$(pwd)}
bindings_root="$top_srcdir/tests/bindings"
status=0

COMMON_IDS="0032 0049 0056 0057 0060 0066 0078 0116 0122 0125 0127"
RUBY_ONLY_IDS="0144 0146 0150 0167 0168"

binding_ext() {
    case "$1" in
        python) printf "%s\n" "py" ;;
        ruby) printf "%s\n" "rb" ;;
        perl) printf "%s\n" "pl" ;;
        php) printf "%s\n" "php" ;;
        *)
            return 1
            ;;
    esac
}

has_loader_setopt_case() {
    binding=$1
    case_id=$2
    ext=$3
    find "$bindings_root/$binding" -maxdepth 1 -type f \
        -name "${case_id}_${binding}_api_loader_setopt*.${ext}" \
        | grep -q .
}

check_case() {
    binding=$1
    case_id=$2
    ext=$3
    if ! has_loader_setopt_case "$binding" "$case_id" "$ext"; then
        echo "error: missing loader_setopt test for ${binding} case ${case_id}" >&2
        status=1
    fi
}

for binding in python ruby perl php; do
    ext=$(binding_ext "$binding")
    for case_id in $COMMON_IDS; do
        check_case "$binding" "$case_id" "$ext"
    done
done

for case_id in $RUBY_ONLY_IDS; do
    check_case ruby "$case_id" rb
done

if [ "$status" -ne 0 ]; then
    exit "$status"
fi

echo "binding loader_setopt case sync: ok"
