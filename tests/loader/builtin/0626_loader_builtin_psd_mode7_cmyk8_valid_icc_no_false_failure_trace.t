#!/bin/sh
# Verify mode7(4ch->CMYK) with valid embedded ICC avoids false failure trace.
# Fixture generation command:
#   python3 - <<'PY'
#   from pathlib import Path
#   import struct
#   base = bytearray(Path("tests/data/inputs/formats/stbi_minimal_cmyk8.psd").read_bytes())
#   icc = Path("/System/Library/ColorSync/Profiles/Generic CMYK Profile.icc").read_bytes()
#   struct.pack_into(">H", base, 24, 7)
#   cml = struct.unpack_from(">I", base, 26)[0]
#   ir_off = 30 + cml
#   lm_off = ir_off + 4 + struct.unpack_from(">I", base, ir_off)[0]
#   block = b"8BIM" + struct.pack(">H", 0x040F) + b"\\x00\\x00" + \
#       struct.pack(">I", len(icc)) + icc + (b"\\x00" if len(icc) & 1 else b"")
#   out = base[:ir_off] + struct.pack(">I", len(block)) + block + base[lm_off:]
#   Path("tests/data/inputs/formats/stbi_minimal_mode7_cmyk8_valid_icc_profile.psd").write_bytes(out)
#   PY

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_cmyk8_valid_icc_profile.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "mode7 CMYK valid ICC decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"embedded ICC conversion failed"*)
        echo "not ok" 1 - "mode7 CMYK valid ICC emitted false failure trace"
        exit 0
        ;;
esac

case "${trace_log}" in
    *"libsixel: loader builtin succeeded"*)
        ;;
    *)
        echo "not ok" 1 - "builtin loader did not finish successfully"
        exit 0
        ;;
esac

echo "ok" 1 - "mode7 CMYK valid ICC path avoids false embedded-ICC failure trace"
exit 0
