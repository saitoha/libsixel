#!/bin/sh
# Verify Python bindings can encode multiple image formats and emit well-formed
# SIXEL streams. The test exercises PNG, JPEG, GIF, and BMP inputs and checks
# that the generated output begins with the DCS introducer and terminates with
# the expected ST sequence.

set -eux

PYTHON_HELPER_DIR="${TOP_SRCDIR}/tests/lib/sh/python"
. "${PYTHON_HELPER_DIR}/common.sh"

tmp_dir="${ARTIFACT_LOCAL_DIR}"
python_prepare "${tmp_dir}"
set -v

verify_script="${tmp_dir}/verify-format.py"
cat >"${verify_script}" <<'PY'
import pathlib
import sys

try:
    from libsixel import SIXEL_OPTFLAG_INPUT
    from libsixel.encoder import Encoder, SIXEL_OPTFLAG_OUTPUT

    def ensure_sixel_signature(path: pathlib.Path) -> int:
        data = path.read_bytes()
        if not data.startswith(b"\x1bPq"):
            raise SystemExit("missing sixel DCS introducer")

        stripped = data.rstrip(b"\r\n")
        if not stripped.endswith(b"\x1b\\"):
            raise SystemExit("missing sixel ST terminator")

        return len(data)


    def encode_image(src: pathlib.Path, dest: pathlib.Path) -> int:
        encoder = Encoder()
        encoder.setopt(SIXEL_OPTFLAG_INPUT, str(src))
        encoder.setopt(SIXEL_OPTFLAG_OUTPUT, str(dest))
        encoder.encode(str(src))

        if not dest.exists():
            raise SystemExit("missing sixel output")
        if dest.stat().st_size == 0:
            raise SystemExit("empty sixel output")

        return ensure_sixel_signature(dest)


    def main() -> None:
        if len(sys.argv) != 3:
            raise SystemExit("usage: verify-format.py <input> <output>")

        source = pathlib.Path(sys.argv[1])
        target = pathlib.Path(sys.argv[2])

        size = encode_image(source, target)
        print(f"encoded {source.name} -> {target.name} ({size} bytes)")


    if __name__ == "__main__":
        main()
except OSError as exc:
    print(f"SKIP_LIBSIXEL_LOAD:{exc}")
    raise SystemExit(2)
PY

# Install the wheel into a venv when available so the API is exercised through
# the packaged module instead of the in-tree sources.
if [ "${use_wheel}" -eq 1 ]; then
    run_venv="${tmp_dir}/venv"
    if ! python_install_wheel "${run_venv}" "${wheel_path}"; then
        tap_skip_all "wheel installation failed"
    fi
fi

formats_count=4
case_id=1

while IFS=: read -r label source_path; do
    output_path="${tmp_dir}/${label}.six"

    if [ "${use_wheel}" -eq 1 ]; then
        python_output=$(env ${python_wheel_loader_env} \
           PYTHONPATH="${python_wheel_trace_pythonpath}" \
           LIBSIXEL_LIBDIR="${python_lib_dir}" \
           "${run_python}" "${verify_script}" \
           "${source_path}" "${output_path}" 2>&1)
        python_status=$?
        printf '%s' "${python_output}" >&2
        if [ "${python_status}" -eq 0 ]; then
            tap_pass ${case_id} "encodes ${label} via wheel (DCS/ST ok)"
        else
            python_skip_on_load_error "${python_status}" "${python_output}"
            tap_fail ${case_id} "${label} encoding via wheel failed"
        fi
    else
        python_output=$(env ${python_in_tree_loader_env} \
           PYTHONPATH="${python_in_tree_trace_pythonpath}" \
           LIBSIXEL_LIBDIR="${python_lib_dir}" \
           "${run_python}" "${verify_script}" \
           "${source_path}" "${output_path}" 2>&1)
        python_status=$?
        printf '%s' "${python_output}" >&2
        if [ "${python_status}" -eq 0 ]; then
            tap_pass ${case_id} "encodes ${label} via in-tree modules (DCS/ST ok)"
        else
            python_skip_on_load_error "${python_status}" "${python_output}"
            tap_fail ${case_id} "${label} encoding via in-tree modules failed"
        fi
    fi

    case_id=$((case_id + 1))
done <<EOF
PNG:${TOP_SRCDIR}/tests/data/inputs/snake_64.png
JPEG:${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg
GIF:${TOP_SRCDIR}/tests/data/inputs/snake_64.gif
BMP:${TOP_SRCDIR}/tests/data/inputs/snake_64.bmp
EOF

tap_plan ${formats_count}
exit ${tap_status}
