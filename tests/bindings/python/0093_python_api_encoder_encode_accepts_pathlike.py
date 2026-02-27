#!/usr/bin/env python3
"""TAP test that encoder encode accepts pathlib.Path input."""

from __future__ import annotations

import os

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder encode accepts pathlib.Path input'


def test_0097_python_api_encoder_encode_accepts_pathlike() -> None:
    import pathlib

    try:
        from libsixel_wheel import SIXEL_OPTFLAG_OUTPUT
        from libsixel_wheel import sixel_encoder_encode
        from libsixel_wheel import sixel_encoder_new
        from libsixel_wheel import sixel_encoder_setopt
        from libsixel_wheel import sixel_encoder_unref
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    source = pathlib.Path(
        os.path.expandvars("${TOP_SRCDIR}/tests/data/inputs/snake_64.png")
    )
    output = pathlib.Path(
        os.path.expandvars("${ARTIFACT_LOCAL_DIR}/encode_pathlike.six")
    )

    encoder = sixel_encoder_new()
    sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, str(output))
    sixel_encoder_encode(encoder, source)
    sixel_encoder_unref(encoder)

    sixel_payload = output.read_bytes()
    if len(sixel_payload) == 0:
        raise SystemExit('encoder output missing for path-like input')

    if not (sixel_payload.startswith(b'\x1bP') and
            sixel_payload.endswith(b'\x1b\\')):
        raise SystemExit('encoder output is not a valid sixel envelope')

    print('encoder path-like input acceptance verified')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0097_python_api_encoder_encode_accepts_pathlike,
    ))
