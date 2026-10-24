#!/usr/bin/env python3
"""TAP test that both thread budget setters validate their inputs."""

from __future__ import annotations

from _taptest import run_embedded_tap_test


DESCRIPTION = 'both thread budget setters validate their inputs'
def test_0035_python_api_thread_budget_setters() -> None:
    try:
        from libsixel_wheel import (sixel_set_encoder_threads,
                                   sixel_set_decoder_threads)
    except (ModuleNotFoundError, OSError) as exc:
        print(f"SKIP_LIBSIXEL_LOAD:{exc}")
        raise SystemExit(2)

    for setter in (sixel_set_encoder_threads, sixel_set_decoder_threads):
        setter(1)
        setter("auto")
        setter("all")
        try:
            setter("invalid")
        except ValueError:
            continue
        raise SystemExit("thread budget API accepted invalid input")
    print("both thread budget APIs accept all and reject invalid input")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0035_python_api_thread_budget_setters))
