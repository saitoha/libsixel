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

if [ -n "$destdir" ]; then
    # Skip the cache update when running inside a staged DESTDIR install.
    # The outer packaging system is responsible for refreshing the cache
    # once the files land on the real root filesystem.
    exit 0
fi

mime_dir="${datadir_path}/mime"
packages_dir="${mime_dir}/packages"

if [ "$mode" = "install" ]; then
    if [ ! -d "$mime_dir" ]; then
        if ! mkdir -p "$mime_dir" 2>/dev/null && [ ! -d "$mime_dir" ]; then
            echo "$0: failed to create '$mime_dir'" >&2
            exit 1
        fi
    fi

    exec "$command_path" "$mime_dir"
fi

if [ ! -d "$mime_dir" ]; then
    exit 0
fi

# Refresh the cache after uninstall so that any remaining MIME definitions
# keep working. When no packages are left, also remove the cache files to
# satisfy distuninstallcheck and avoid stale caches for a clean prefix.
"$command_path" "$mime_dir"

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
rmdir "${mime_dir}/icons" 2>/dev/null || true
rmdir "$mime_dir" 2>/dev/null || true
