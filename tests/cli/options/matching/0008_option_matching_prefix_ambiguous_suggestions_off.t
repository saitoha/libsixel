#!/bin/sh
# TAP test verifying prefix suggestions can be disabled for ambiguity errors.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="prefix_ambiguous_suggestions_off"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

rm -f "${err_file}" "${out_file}"

if run_img2sixel --env SIXEL_OPTION_PREFIX_SUGGESTIONS=0 -- \
        -d sie "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        >"${out_file}" 2>"${err_file}"; then
    fail 1 "ambiguous prefix unexpectedly succeeded"
    exit 0
fi

if grep -F 'ambiguous prefix "sie".' "${err_file}" >/dev/null 2>&1 &&
   ! grep -F '(matches:' "${err_file}" >/dev/null 2>&1; then
    pass 1 "ambiguity diagnostic omits candidate list when disabled"
else
    fail 1 "ambiguity diagnostic still contains candidate list"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
fi

exit 0
