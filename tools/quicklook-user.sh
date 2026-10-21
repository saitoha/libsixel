#!/bin/sh
# Explicit user/lifecycle entry shared by both build systems. No privilege
# inference or elevation occurs here; the operator supplies the exact inputs.
set -eu
test "$#" = 3 || { echo 'usage: quicklook-user.sh ACTION DISTRIBUTION TOOL' >&2; exit 2; }
action=$1
distribution=$2
tool=$3
test -z "${DESTDIR:-}${SIXEL_INSTALLCHECK_DESTDIR:-}" || {
    echo 'Quick Look user/lifecycle operations reject STAGING' >&2
    exit 2
}
case "$action" in
    activate|set-default|deactivate)
        set -- "$tool" user "$action" "$distribution" \
            "${QUICKLOOK_USER_REQUEST:?Explicit request JSON required}" \
            "${QUICKLOOK_USER_JOURNAL:?Private journal directory required}"
        ;;
    migration-activate|migration-prepare|migration-restore|migration-verify)
        set -- "$tool" migration "${action#migration-}" "$distribution" \
            "${QUICKLOOK_USER_REQUEST:?Explicit migration request required}" \
            "${QUICKLOOK_USER_JOURNAL:?Private journal directory required}"
        ;;
    archive-copy|remove-copy|restore-copy)
        set -- "$tool" legacy "${action%-copy}" "$distribution" \
            "${QUICKLOOK_USER_REQUEST:?Explicit request JSON required}" \
            "${QUICKLOOK_USER_JOURNAL:?Private journal directory required}"
        ;;
    remove-users)
        set -- "$tool" remove-users "$distribution" \
            "${QUICKLOOK_PARTICIPANTS:?Explicit participant evidence required}"
        ;;
    lifecycle)
        set -- "$tool" lifecycle \
            "${QUICKLOOK_LIFECYCLE_PLAN:?Explicit lifecycle plan required}" \
            "${QUICKLOOK_LIFECYCLE_RECORD:?Private run directory required}"
        ;;
    *) echo 'Unknown Quick Look user/lifecycle operation' >&2; exit 2 ;;
esac
case "${QUICKLOOK_DRY_RUN:-no}" in
    yes) printf '%s\n' "$@"; exit 0 ;;
    no) ;;
    *) echo 'QUICKLOOK_DRY_RUN must be yes or no' >&2; exit 2 ;;
esac
exec "$@"
