#!/bin/sh
# Install an Emscripten .wasm sidecar next to the installed JS wrapper.
set -eu

if test "$#" -ne 3; then
    echo "usage: $0 WRAPPER_PATH INSTALL_DIR MODULE_NAME" >&2
    exit 1
fi

wrapper_path="$1"
install_dir="$2"
module_name="$3"
wasm_path="${wrapper_path}.wasm"

# SINGLE_FILE=1 builds do not emit a sidecar.
if test ! -f "$wasm_path"; then
    exit 0
fi

if test "${install_dir#/}" = "$install_dir"; then
    if test -n "${MESON_INSTALL_DESTDIR_PREFIX-}"; then
        target_dir="${MESON_INSTALL_DESTDIR_PREFIX}/${install_dir}"
    elif test -n "${MESON_INSTALL_PREFIX-}"; then
        target_dir="${DESTDIR-}${MESON_INSTALL_PREFIX}/${install_dir}"
    else
        target_dir="${DESTDIR-}/${install_dir}"
    fi
else
    target_dir="${DESTDIR-}${install_dir}"
fi

mkdir -p "$target_dir"
install -m 644 "$wasm_path" "${target_dir}/${module_name}.wasm"
