#!/usr/bin/env python3
# Policy: docs/cli/threading-policy.md
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
        for value in (1, "2", "all", " ALL ", b" AlL "):
            setter(value)
        for value in ("invalid", 0, -1, "0", "1.5", None):
            try:
                setter(value)
            except ValueError:
                continue
            raise SystemExit("thread budget API accepted invalid input")
    print("both thread budget APIs accept all and reject invalid input")


if __name__ == "__main__":
    raise SystemExit(run_embedded_tap_test(DESCRIPTION, test_0035_python_api_thread_budget_setters))
