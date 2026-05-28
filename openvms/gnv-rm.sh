#!/bin/sh
#
# Normalize OpenVMS/GNV rm cleanup semantics for generated build scripts.
#
# GNV rm.exe can return an OpenVMS warning condition for best-effort cleanup
# patterns that POSIX build tools expect to be harmless, especially "rm -f" on
# operands that are already absent.  Some GNV shells can preserve that warning
# as the image status even after the script later exits 0, so avoid invoking
# rm.exe for operands that should be ignored.

set -u

rm_prog=${GNV_RM:-rm.exe}
rm_force=no
rm_recursive=no
rm_options=
rm_status=0
rm_saw_operand=no
rm_end_options=no

rm_path_exists()
{
    test -f "$1" || test -d "$1" || test -L "$1"
}

for rm_arg in "$@"; do
    if test "$rm_end_options" = no; then
        case "$rm_arg" in
          --)
            rm_end_options=yes
            continue
            ;;
          -*)
            rm_options="$rm_options $rm_arg"
            case "$rm_arg" in
              -*f*) rm_force=yes ;;
            esac
            case "$rm_arg" in
              -*[rR]*) rm_recursive=yes ;;
            esac
            continue
            ;;
        esac
    fi

    rm_saw_operand=yes

    case "$rm_arg" in
      *[\*\?\[]*)
        if test "$rm_force" = yes && ! rm_path_exists "$rm_arg"; then
            continue
        fi
        ;;
    esac

    if test "$rm_force" = yes &&
       test "$rm_recursive" != yes &&
       test -d "$rm_arg"; then
        continue
    fi

    # Deliberately leave rm_options unquoted so combined options such as
    # "-f -r" remain separate arguments when passed to rm.exe.
    # shellcheck disable=SC2086
    "$rm_prog" $rm_options "$rm_arg"
    rm_one_status=$?

    if test "$rm_one_status" -ne 0; then
        if test "$rm_force" = yes && ! rm_path_exists "$rm_arg"; then
            continue
        fi
        rm_status=$rm_one_status
    fi
done

if test "$rm_saw_operand" = no; then
    test "$rm_force" = yes && exit 0
    # shellcheck disable=SC2086
    "$rm_prog" $rm_options
    exit $?
fi

exit "$rm_status"
