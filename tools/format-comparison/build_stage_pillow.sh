#!/bin/sh
# Build a stage-timed Pillow that matches the wheel used by measure.py.
#
# usage: build_stage_pillow.sh WHEEL_PYTHON TARGET_VENV
#
# WHEEL_PYTHON is the interpreter whose Pillow wheel the study measures. The
# script builds the same Pillow version from source with
# pillow_stage_timers.py into a new virtual environment TARGET_VENV, and on
# macOS binds it to the wheel's own bundled zlib so DEFLATE is the measured
# implementation. stages.py rejects the build if its GIF or PNG bytes differ.
set -eu
wheel_python=${1:?usage: build_stage_pillow.sh WHEEL_PYTHON TARGET_VENV}
target=${2:?usage: build_stage_pillow.sh WHEEL_PYTHON TARGET_VENV}
here=$(CDPATH='' cd -- "${0%/*}" && pwd -P)
version=$("${wheel_python}" -c 'import PIL; print(PIL.__version__)')
work=$(mktemp -d "${TMPDIR-/tmp}/stage-pillow.XXXXXX")
"${wheel_python}" -m venv "${target}"
"${target}/bin/pip" download --no-binary :all: --no-deps -d "${work}" \
    "pillow==${version}"
tar -xzf "${work}/pillow-${version}.tar.gz" -C "${work}"
"${wheel_python}" "${here}/pillow_stage_timers.py" "${work}/pillow-${version}"
# Only the GIF and PNG paths are measured; skip every optional codec.
LDFLAGS=-Wl,-headerpad_max_install_names "${target}/bin/pip" install \
    --no-deps "${work}/pillow-${version}" \
    -C jpeg=disable -C tiff=disable -C freetype=disable -C lcms=disable \
    -C webp=disable -C jpeg2000=disable -C imagequant=disable \
    -C xcb=disable -C avif=disable
case $(uname -s) in
    Darwin)
        wheel_pil=$("${wheel_python}" -c \
            'import os, PIL; print(os.path.dirname(PIL.__file__))')
        target_pil=$("${target}/bin/python" -c \
            'import os, PIL; print(os.path.dirname(PIL.__file__))')
        zlib=$(cd "${wheel_pil}/.dylibs" && ls libz.*.dylib)
        mkdir -p "${target_pil}/.dylibs"
        cp "${wheel_pil}/.dylibs/${zlib}" "${target_pil}/.dylibs/"
        for module in "${target_pil}"/_imaging.*.so; do
            old=$(otool -L "${module}" | awk '/libz\./ { print $1 }')
            install_name_tool -change "${old}" \
                "@loader_path/.dylibs/${zlib}" "${module}"
            codesign -f -s - "${module}"
        done
        ;;
esac
"${target}/bin/python" -c \
    'from PIL import _imaging, features; _imaging._stage_times(); print(features.version("zlib"))'
