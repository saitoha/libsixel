#!/bin/bash
# Common package management entry. No user inference, signing or registration.
set -eu

usage() {
    echo "usage: $0 install|uninstall|installcheck --package-dir DIR --tool PATH [--datadir DIR] [--destdir DIR] [--dry-run] [--users-deactivated]" >&2
    exit 2
}

test "$#" -gt 0 || usage
action=$1
shift
case "$action" in install|uninstall|installcheck) ;; *) usage ;; esac
package_dir=''
tool=''
datadir=/usr/local/share
destdir=${DESTDIR:-}
dry_run=${MESON_INSTALL_DRY_RUN:-0}
users_deactivated=${QUICKLOOK_USERS_DEACTIVATED:-no}
while test "$#" -gt 0; do
    case "$1" in
        --package-dir|--tool|--datadir|--destdir)
            test "$#" -ge 2 || usage
            case "$1" in
                --package-dir) package_dir=$2 ;;
                --tool) tool=$2 ;;
                --datadir) datadir=$2 ;;
                --destdir) destdir=$2 ;;
            esac
            shift 2 ;;
        --dry-run) dry_run=1; shift ;;
        --users-deactivated) users_deactivated=yes; shift ;;
        *) usage ;;
    esac
done
test -n "$package_dir" && test -n "$tool" || usage
# Match the generic installed-command harness's alternate staging variable.
if test "$action" = installcheck && test -z "$destdir"; then
    destdir=${SIXEL_INSTALLCHECK_DESTDIR:-}
fi
# Meson skips install scripts on dry-run; this also protects explicit calls.
if test "$dry_run" = 1; then
    printf 'Quick Look dry-run: %s package=%s DESTDIR=%s\n' "$action" "$package_dir" "$destdir"
    exit 0
fi
exec "$tool" manage "$action" "$package_dir" "$datadir" "$destdir" "$users_deactivated"
