#!/bin/sh
# TAP test verifying that converters/img2sixel.c:g_env_help_table documents
# every environment variable referenced in the sources.

set -eux


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

"${TOP_SRCDIR}/tests/docs/consistency/list_envvars.sh" --check \
        --source-root "${TOP_SRCDIR}" > "${ARTIFACT_LOCAL_DIR}/output.txt" || {
    printf 'not ok 1 - mismatch between sources and env help table\n'
    exit 0
}

printf 'ok 1 - environment variables match between sources and env help table\n'
exit 0
