#!/bin/sh
# TAP test verifying SIXEL_LOADER_BUILTIN_CMS_ENGINE overrides global setting.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/map8_embedded_icc.png"
output_cms1="${ARTIFACT_LOCAL_DIR}/cms_engine_env_builtin_ref_cms1.six"
output_cms0="${ARTIFACT_LOCAL_DIR}/cms_engine_env_builtin_ref_cms0.six"
output_override="${ARTIFACT_LOCAL_DIR}/cms_engine_env_builtin_override.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! "${input_png}" >"${output_cms1}" || {
    echo "not ok" 1 - "builtin cms=1 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! "${input_png}" >"${output_cms0}" || {
    echo "not ok" 1 - "builtin cms=0 reference decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env "SIXEL_LOADER_CMS_ENGINE=none" \
    --env "SIXEL_LOADER_BUILTIN_CMS_ENGINE=auto" \
    -Lbuiltin! "${input_png}" >"${output_override}" || {
    echo "not ok" 1 - "SIXEL_LOADER_BUILTIN_CMS_ENGINE override decode failed"
    exit 0
}

# Precedence is an exact output relationship, not a perceptual threshold.
test -s "${output_override}" || {
    echo "not ok 1 - encoded output is empty"
    exit 0
}
cmp -s "${output_override}" "${output_cms1}" || {
    echo "not ok 1 - per-loader override must match explicit CMS selection"
    exit 0
}
cmp -s "${output_override}" "${output_cms0}" && {
    echo "not ok 1 - per-loader override must differ from disabled CMS"
    exit 0
}

echo "ok" 1 - "SIXEL_LOADER_BUILTIN_CMS_ENGINE overrides global none"
exit 0
