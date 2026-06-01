#!/bin/sh
#
# Link an OpenVMS executable from Automake program-link arguments.
#
# GNV gcc cannot reliably produce an executable when the final link command is
# made only from existing object files and archives.  This wrapper keeps the
# normal Autotools compile path, converts the final object/archive list to a
# DCL LINK option file, and asks the native OpenVMS linker to create the image.

set -eu

prog=${0##*/}
out=
input_count=0
opt_file=${TMPDIR:-.}/libsixel-gnv-link-$$.opt
link_log=${TMPDIR:-.}/libsixel-gnv-link-$$.log
archive_members_file=${TMPDIR:-.}/libsixel-gnv-link-$$.members
archive_objects_file=${TMPDIR:-.}/libsixel-gnv-link-$$.objects

trap 'rm -f "$opt_file" "$link_log" "$archive_members_file" "$archive_objects_file"' 0 1 2 3 15
: > "$opt_file"

die()
{
    echo "$prog: $*" >&2
    exit 1
}

find_realpath()
{
    if command -v realpath.exe >/dev/null 2>&1; then
        echo realpath.exe
        return
    fi
    if command -v realpath >/dev/null 2>&1; then
        echo realpath
        return
    fi
    die "realpath.exe was not found"
}

find_ar()
{
    if command -v ar.exe >/dev/null 2>&1; then
        echo ar.exe
        return
    fi
    if command -v ar >/dev/null 2>&1; then
        echo ar
        return
    fi
    return 1
}

escape_vms_dir_component()
{
    # A POSIX directory named ".libs" must become "^.libs" inside a VMS
    # directory specification; otherwise the dot is parsed as a separator.
    printf '%s\n' "$1" | sed 's/\./^./g'
}

absolute_posix_path()
{
    path=$1
    realpath_cmd=$2

    case "$path" in
      */*)
        path_dir=${path%/*}
        path_base=${path##*/}
        ;;
      *)
        path_dir=.
        path_base=$path
        ;;
    esac

    if test -z "$path_dir"; then
        path_dir=/
    fi

    resolved_dir=`"$realpath_cmd" "$path_dir"` ||
        die "cannot resolve directory for $path"
    printf '%s/%s\n' "$resolved_dir" "$path_base"
}

posix_to_vms()
{
    path=$1
    realpath_cmd=$2
    abs_path=`absolute_posix_path "$path" "$realpath_cmd"`

    case "$abs_path" in
      /*/*)
        ;;
      *)
        die "cannot convert path to VMS syntax: $path"
        ;;
    esac

    without_slash=${abs_path#/}
    logical=${without_slash%%/*}
    rest=${without_slash#*/}
    file=${rest##*/}
    dirs=${rest%/*}

    if test "x$dirs" = "x$rest"; then
        printf '%s:%s\n' "$logical" "$file"
        return
    fi

    vms_dirs=
    old_ifs=$IFS
    IFS=/
    for dir_component in $dirs; do
        dir_component=`escape_vms_dir_component "$dir_component"`
        if test -z "$vms_dirs"; then
            vms_dirs=$dir_component
        else
            vms_dirs=$vms_dirs.$dir_component
        fi
    done
    IFS=$old_ifs

    printf '%s:[%s]%s\n' "$logical" "$vms_dirs" "$file"
}

add_link_input()
{
    path=$1
    suffix=$2
    realpath_cmd=$3

    if test ! -f "$path"; then
        die "link input does not exist: $path"
    fi

    vms_path=`posix_to_vms "$path" "$realpath_cmd"`
    printf '%s%s\n' "$vms_path" "$suffix" >> "$opt_file"
    input_count=$((input_count + 1))
}

resolve_archive_member()
{
    member=$1
    archive_dir=$2
    la_dir=$3

    # GNU ar may store only the basename of a libtool object.  The
    # amalgamated OpenVMS build compiles src/libsixel.la from
    # ../amalgamation, so check that sibling build directory explicitly.
    for candidate in \
        "$archive_dir/$member" \
        "$la_dir/$member" \
        "$la_dir/../amalgamation/$member" \
        "$member"; do
        if test -f "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

resolve_archive_member_objects()
{
    archive_file=$1
    la_dir=$2
    ar_cmd=$3
    resolved_count=0

    case "$archive_file" in
      */*) archive_dir=${archive_file%/*} ;;
      *) archive_dir=. ;;
    esac

    : > "$archive_members_file"
    : > "$archive_objects_file"
    "$ar_cmd" t "$archive_file" > "$archive_members_file" 2>/dev/null ||
        return 1
    test -s "$archive_members_file" || return 1

    while IFS= read -r member || test -n "$member"; do
        test -n "$member" || continue
        case "$member" in
          *.o|*.obj)
            ;;
          *)
            return 1
            ;;
        esac
        member_path=`resolve_archive_member "$member" "$archive_dir" "$la_dir"` ||
            return 1
        printf '%s\n' "$member_path" >> "$archive_objects_file"
        resolved_count=$((resolved_count + 1))
    done < "$archive_members_file"

    test "$resolved_count" -gt 0
}

add_archive_member_objects()
{
    archive_file=$1
    la_dir=$2
    realpath_cmd=$3
    ar_cmd=`find_ar` || return 1

    resolve_archive_member_objects "$archive_file" "$la_dir" "$ar_cmd" ||
        return 1
    while IFS= read -r member_path || test -n "$member_path"; do
        test -n "$member_path" || continue
        add_link_input "$member_path" "" "$realpath_cmd"
    done < "$archive_objects_file"
}

read_la_value()
{
    key=$1
    la_file=$2

    sed -n "s/^$key='\\(.*\\)'/\\1/p" "$la_file" | sed 1q
}

add_libtool_archive()
{
    la_file=$1
    realpath_cmd=$2
    old_library=`read_la_value old_library "$la_file"`

    if test -z "$old_library"; then
        die "libtool archive has no old_library: $la_file"
    fi

    case "$la_file" in
      */*) la_dir=${la_file%/*} ;;
      *) la_dir=. ;;
    esac

    archive_file=$la_dir/.libs/$old_library

    # OpenVMS LINK can leave unresolved references when a libtool archive
    # contains a single large amalgamation object and is passed as /LIBRARY.
    # When the archive members still exist on disk, list those objects
    # directly in the option file so LINK does not have to perform archive
    # member selection.
    if add_archive_member_objects "$archive_file" "$la_dir" "$realpath_cmd"; then
        return
    fi

    add_link_input "$archive_file" "/LIBRARY" "$realpath_cmd"
}

realpath_cmd=`find_realpath`

while test "$#" -gt 0; do
    arg=$1
    shift

    case "$arg" in
      -o)
        test "$#" -gt 0 || die "-o requires an output path"
        out=$1
        shift
        ;;
      -o*)
        out=${arg#-o}
        ;;
      *.la)
        add_libtool_archive "$arg" "$realpath_cmd"
        ;;
      *.a|*.olb)
        add_link_input "$arg" "/LIBRARY" "$realpath_cmd"
        ;;
      *.o|*.obj)
        add_link_input "$arg" "" "$realpath_cmd"
        ;;
      -l)
        test "$#" -gt 0 || die "-l requires a library name"
        lib=$1
        shift
        case "$lib" in
          m|c) ;;
          *) die "unsupported OpenVMS library option: -l $lib" ;;
        esac
        ;;
      -lm|-lc)
        ;;
      -l*)
        die "unsupported OpenVMS library option: $arg"
        ;;
      -L*|-Wl,*|--coverage|-g|-O*|-D*|-I*|-pthread)
        ;;
      "")
        ;;
      *)
        if test -f "$arg"; then
            add_link_input "$arg" "" "$realpath_cmd"
        else
            die "unsupported link argument: $arg"
        fi
        ;;
    esac
done

test -n "$out" || die "missing -o output path"
test "$input_count" -gt 0 || die "no object or archive inputs"

vms_out=`posix_to_vms "$out" "$realpath_cmd"`
vms_opt=`posix_to_vms "$opt_file" "$realpath_cmd"`

set +e
/bin/dcl.exe "link/executable=$vms_out $vms_opt/options" > "$link_log" 2>&1
link_status=$?
set -e

cat "$link_log"

if grep '^%ILINK-W-NUDFSYMS' "$link_log" >/dev/null 2>&1 ||
   grep '^%ILINK-W-USEUNDEF' "$link_log" >/dev/null 2>&1; then
    die "OpenVMS LINK left undefined symbols while linking $out"
fi

if test -f "$out"; then
    exit 0
fi

case "$out" in
  *.exe) out_alt=${out%.exe} ;;
  *) out_alt=$out.exe ;;
esac

if test -f "$out_alt"; then
    exit 0
fi

if test "$link_status" -ne 0; then
    die "OpenVMS LINK exited with status $link_status and did not create $out"
fi

die "OpenVMS LINK did not create $out"
