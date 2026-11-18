#!/bin/sh
# The common upload must retain hidden-directory fuzz replay artifacts.
set -eux
echo '1..1'
set -v
src_root=$1
test -f "$src_root/.github/workflows/triage.yml" || {
    echo 'ok 1 # SKIP workflow checkout required'
    exit 0
}
awk '
    /- name: Upload fuzz replay artifacts$/ { active = 1; next }
    active && /^      - / { active = 0 }
    active && /uses: actions\/upload-artifact@/ { uploader = 1 }
    active && /^[[:space:]]*if: always\(\)$/ { always = 1 }
    active && /include-hidden-files: true$/ { hidden = 1 }
    active && /^[[:space:]]*\.tmp\/fuzz-artifacts-\*$/ { artifacts = 1 }
    END { exit !(uploader && always && hidden && artifacts) }
' "$src_root/.github/workflows/triage.yml" || {
    echo 'not ok 1 - common fuzz replay artifacts are always uploaded'
    exit 0
}
echo 'ok 1 - common fuzz replay artifacts are always uploaded'
exit 0
