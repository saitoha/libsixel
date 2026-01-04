#!/bin/sh
# Run update-mime-database for libsixel thumbnailer installs.

set -eu

# Meson sets MESON_INSTALL_DRY_RUN when probing the install step, so do
# nothing in that mode to keep the checks side-effect free.
if [ "${MESON_INSTALL_DRY_RUN:-0}" = "1" ]; then
    exit 0
fi

if [ $# -ne 3 ]; then
    echo "usage: $0 COMMAND DATADIR MODE" >&2
    exit 1
fi

command_path=$1
datadir_path=$2
mode=$3

if [ -z "$command_path" ]; then
    exit 0
fi

case "$mode" in
    install|uninstall)
        ;;
    *)
        echo "$0: unknown mode '$mode'" >&2
        exit 1
        ;;
esac

destdir=${DESTDIR:-}
if [ -z "$destdir" ]; then
    destdir=${MESON_INSTALL_DESTDIR:-}
fi

mime_dir="${datadir_path}/mime"
packages_dir="${mime_dir}/packages"

if [ "$mode" = "install" ]; then
    if [ -n "$destdir" ]; then
        # Skip the cache update when running inside a staged DESTDIR install.
        # The outer packaging system is responsible for refreshing the cache
        # once the files land on the real root filesystem.
        exit 0
    fi

    if [ ! -d "$mime_dir" ]; then
        if ! mkdir -p "$mime_dir" 2>/dev/null && [ ! -d "$mime_dir" ]; then
            echo "$0: failed to create '$mime_dir'" >&2
            exit 1
        fi
    fi

    exec "$command_path" "$mime_dir"
fi

# Refresh the cache after uninstall so that any remaining MIME definitions
# keep working. When no packages are left, also remove the cache files to
# satisfy distuninstallcheck and avoid stale caches for a clean prefix.
if [ -z "$destdir" ]; then
    # Refresh the cache after uninstall on the real root so that any
    # remaining MIME definitions keep working.
    "$command_path" "$mime_dir"
fi

staged_mime_dir=$mime_dir
staged_packages_dir=$packages_dir

if [ -n "$destdir" ]; then
    # When uninstalling under DESTDIR (e.g. distcheck), clean the staged
    # prefix to avoid leftovers while keeping the real prefix untouched.
    staged_mime_dir="${destdir}${mime_dir}"
    staged_packages_dir="${destdir}${packages_dir}"
fi

mime_dir=$staged_mime_dir
packages_dir=$staged_packages_dir

if [ ! -d "$mime_dir" ]; then
    exit 0
fi

if [ -d "$packages_dir" ]; then
    if find "$packages_dir" -type f -print -quit 2>/dev/null | grep -q .;
    then
        exit 0
    fi
fi

rm -f "${mime_dir}/aliases" \
    "${mime_dir}/generic-icons" \
    "${mime_dir}/globs" \
    "${mime_dir}/globs2" \
    "${mime_dir}/magic" \
    "${mime_dir}/mime.cache" \
    "${mime_dir}/subclasses" \
    "${mime_dir}/treemagic" \
    "${mime_dir}/types" \
    "${mime_dir}/version" \
    "${mime_dir}/XMLnamespaces"

rmdir "$packages_dir" 2>/dev/null || true

# update-mime-database leaves an icons/ tree even when no packages remain.
# Remove it once packages/ is empty so distuninstallcheck sees a clean
# uninstall, but leave it intact when other packages still exist.
if [ -d "${mime_dir}/icons" ]; then
    rm -rf "${mime_dir}/icons"
fi

# remove all generated MIME data (e.g., image/x-sixel.xml) once no packages
# remain; this keeps staged DESTDIR trees clean for distuninstallcheck
find "$mime_dir" -mindepth 1 -maxdepth 1 -exec rm -rf {} +

rmdir "$mime_dir" 2>/dev/null || true
