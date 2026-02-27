#!/bin/sh
# TAP test verifying prefix suggestions can be disabled for ambiguity errors.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

label="prefix_ambiguous_suggestions_off"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

: >"${err_file}"
: >"${out_file}"

run_img2sixel --env SIXEL_OPTION_PREFIX_SUGGESTIONS=0 \
    -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    >"${out_file}" 2>"${err_file}" && {
    fail 1 "ambiguous prefix unexpectedly succeeded"
    exit 0
}

grep 'ambiguous prefix "sie".' "${err_file}" >/dev/null 2>&1 || {
    fail 1 "ambiguity diagnostic still contains candidate list"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

grep '(matches:' "${err_file}" >/dev/null 2>&1 && {
    fail 1 "ambiguity diagnostic still contains candidate list"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
    exit 0
}

pass 1 "ambiguity diagnostic omits candidate list when disabled"
exit 0
