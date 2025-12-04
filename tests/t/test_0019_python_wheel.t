#!/bin/sh
# TAP test validating Python wheel build/install before system installation.
#
# The test ensures the source tree can generate a wheel, install it into an
# isolated virtual environment, and import the bindings while using the
# freshly-built libsixel shared library from the build directory. This guards
# against regressions in the packaging workflow used by distributors.

set -eu

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

skip_all() {
    printf '1..0 # SKIP %s\n' "$1"
    exit 0
}

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/wheel.log"
tmp_dir="${artifact_dir}/tmp"
dist_dir="${artifact_dir}/dist"
build_venv="${tmp_dir}/build-venv"
build_python="${build_venv}/bin/python"
verify_script="${tmp_dir}/verify-bindings.py"

mkdir -p "${artifact_dir}" "${tmp_dir}" "${dist_dir}"

python_bin=$(command -v python3 || true)
if [ -z "${python_bin}" ]; then
    skip_all "python3 is not available"
fi

if [ ! -f "${TOP_BUILDDIR}/python/Makefile" ] && \
   [ ! -d "${TOP_BUILDDIR}/python" ]; then
    skip_all "python bindings are disabled"
fi

# Require local venv/ensurepip and wheel modules so we can skip gracefully
# on minimal Python installations rather than failing the entire suite.
if "${python_bin}" - <<'PY' >>"${log_file}" 2>&1; then
import importlib.util
missing = [m for m in ("venv", "ensurepip")
           if importlib.util.find_spec(m) is None]
if missing:
    raise SystemExit(f"missing modules: {', '.join(missing)}")
PY
    :
else
    skip_all "python3 lacks venv or ensurepip support"
fi

if "${python_bin}" - <<'PY' >>"${log_file}" 2>&1; then
import importlib.util
if importlib.util.find_spec("wheel") is None:
    raise SystemExit("wheel module missing")
PY
    :
else
    skip_all "python3 lacks wheel module"
fi

lib_paths=""
for candidate in "${TOP_BUILDDIR}/src/.libs" \
                 "${TOP_BUILDDIR}/src" \
                 "${TOP_BUILDDIR}/src/libsixel"; do
    if [ -d "${candidate}" ]; then
        if [ -z "${lib_paths}" ]; then
            lib_paths="${candidate}"
        else
            lib_paths="${lib_paths}:${candidate}"
        fi
    fi
done

if [ -z "${lib_paths}" ]; then
    first_match=$(find "${TOP_BUILDDIR}/src" -maxdepth 4 -type f \
        \( -name 'libsixel*.so*' -o -name 'libsixel*.dylib' \
           -o -name 'libsixel*.dll' \) | head -n 1 || true)
    if [ -n "${first_match}" ]; then
        lib_paths=$(dirname "${first_match}")
    fi
fi

if [ -z "${lib_paths}" ]; then
    skip_all "compiled libsixel library is unavailable"
fi

export LD_LIBRARY_PATH="${lib_paths}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export DYLD_LIBRARY_PATH="${lib_paths}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"

cat >"${verify_script}" <<'PY'
import libsixel

encoder = libsixel.sixel_encoder_new()
libsixel.sixel_encoder_unref(encoder)
print("bindings loaded")
PY

echo "1..3"
status=0
case_id=1

if "${python_bin}" -m venv "${build_venv}" >>"${log_file}" 2>&1 && \
   "${build_python}" -m pip install --upgrade pip setuptools wheel \
        >>"${log_file}" 2>&1 && \
   "${build_python}" -m pip wheel "${TOP_SRCDIR}/python" \
        --no-deps --no-build-isolation --wheel-dir "${dist_dir}" \
        >"${log_file}" 2>&1; then
    pass ${case_id} "builds python wheel"
else
    fail ${case_id} "python wheel build failed"
fi
case_id=$((case_id + 1))

if "${python_bin}" -m venv "${tmp_dir}/venv" >>"${log_file}" 2>&1 && \
   "${tmp_dir}/venv/bin/python" -m pip install --no-deps --no-index \
        --find-links "${dist_dir}" libsixel-python >>"${log_file}" 2>&1; then
    pass ${case_id} "installs wheel into virtualenv"
else
    fail ${case_id} "wheel installation failed"
fi
case_id=$((case_id + 1))

if LD_LIBRARY_PATH="${LD_LIBRARY_PATH}" \
   DYLD_LIBRARY_PATH="${DYLD_LIBRARY_PATH}" \
   "${tmp_dir}/venv/bin/python" "${verify_script}" >>"${log_file}" 2>&1; then
    pass ${case_id} "imports bindings and initializes encoder"
else
    fail ${case_id} "python import or encoder init failed"
fi

exit ${status}
