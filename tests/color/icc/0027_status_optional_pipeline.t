#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify that successful open reports the skipped pipeline.

set -eux

echo "1..1"
set -v

trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    icc/0027_status_optional_pipeline 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' "not ok 1 - successful open reports the skipped pipeline"
    exit 0
}
case "$trace" in *"LSXCMS2|event=profile-open|first=builtin|engine=builtin|reason=ok|first_failure=ok|skipped_path=unsupported-profile"*) true ;; *) false ;; esac || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' "not ok 1 - successful open reports the skipped pipeline"
    exit 0
}

printf '\n%s\n' "ok 1 - successful open reports the skipped pipeline"
exit 0
