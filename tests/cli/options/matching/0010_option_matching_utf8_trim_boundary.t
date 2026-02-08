#!/bin/sh
# TAP test verifying long UTF-8 diagnostics stay valid after status truncation.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

label="utf8_trim_boundary"
err_file="${ARTIFACT_LOCAL_DIR}/${label}.err"
out_file="${ARTIFACT_LOCAL_DIR}/${label}.sixel"

rm -f "${err_file}" "${out_file}"

long_utf8=$(python3 - <<'PY'
import sys
sys.stdout.buffer.write(("あ" * 1800).encode("utf-8"))
PY
)

if run_img2sixel -r "${long_utf8}" \
        "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
        >"${out_file}" 2>"${err_file}"; then
    fail 1 "long UTF-8 invalid argument unexpectedly succeeded"
    exit 0
fi

if python3 - "${err_file}" <<'PY'
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
path.read_bytes().decode('utf-8')
PY
then
    pass 1 "long UTF-8 diagnostics remain valid UTF-8"
else
    fail 1 "long UTF-8 diagnostics were truncated mid-sequence"
    printf '%s\n' '--- stderr ---' >&2
    cat "${err_file}" >&2 2>/dev/null || :
fi

exit 0
