#!/usr/bin/env python3
# Policy: docs/cli/threading-policy.md
"""TAP test that both thread budget setters reject auto spellings."""

from __future__ import annotations

from _taptest import run_embedded_tap_test

DESCRIPTION = 'both thread budget setters reject auto'


def test_0170_python_api_thread_budget_setters_reject_auto() -> None:
    from libsixel_wheel import (sixel_set_encoder_threads,
                               sixel_set_decoder_threads)

    for setter in (sixel_set_encoder_threads, sixel_set_decoder_threads):
        for value in ('auto', 'AUTO', ' AuTo ', b'auto', b' AUTO '):
            try:
                setter(value)
            except ValueError as exc:
                assert str(exc) == "threads must be a positive integer or 'all'"
                continue
            raise SystemExit('thread budget API accepted auto')


if __name__ == '__main__':
    raise SystemExit(run_embedded_tap_test(
        DESCRIPTION, test_0170_python_api_thread_budget_setters_reject_auto,
    ))
