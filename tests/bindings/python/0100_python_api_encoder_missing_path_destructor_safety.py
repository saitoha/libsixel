#!/usr/bin/env python3
"""TAP test that encoder destructor stays safe after missing path error."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'encoder destructor stays safe after missing path error'


def test_0103_python_api_encoder_missing_path_destructor_safety() -> None:
    import gc

    try:
        from libsixel_wheel.encoder import Encoder
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    encoder = Encoder()
    rejected = False

    try:
        encoder.encode('tests/data/inputs/formats/this_file_does_not_exist.png')
    except RuntimeError:
        rejected = True

    if not rejected:
        raise SystemExit('encoder accepted missing input path')

    # Keep this object going out of scope to exercise the Python-side
    # destructor path after a C-side encode error.
    del encoder
    gc.collect()

    print('encoder destructor remained stable after missing path error')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION,
        test_0103_python_api_encoder_missing_path_destructor_safety,
    ))
