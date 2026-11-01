# GNU/Hurd Compatibility

## Scope

GNU/Hurd is a distinct target platform, not a GNU/Linux variant. The maintained amd64 build identifies itself with the `x86_64-unknown-gnu0.9` host triplet. Most source compatibility remains capability-driven; the narrow `__GNU__` branch described below handles an initializer representation that cannot be selected with a configure-style feature probe. This document owns those portability rules and the GNU/Hurd-specific CI runtime constraints. The complete inventory is in the [platform compatibility ledger](README.md).

## Source compatibility contract

Do not infer a Linux kernel from GCC, GNU userland tools, glibc, or the `gnu` component of the host triplet. Kernel interfaces selected by `__linux__` remain Linux-only. Code shared with GNU/Hurd should instead follow the configure or Meson result for the header, declaration, function, library, or POSIX facility it needs.

GNU/Hurd defines `PTHREAD_ONCE_INIT` as a compound literal. GCC accepts that form as a file-scope initializer, but diagnoses it as non-constant under the Meson warning policy's `-Wpedantic -Werror` combination. All internal pthread once objects must therefore be declared with `SIXEL_PTHREAD_ONCE_DECLARE(name)` from [`src/pthread-once.h`](../../../src/pthread-once.h). On GNU/Hurd, that wrapper prefixes the complete declaration with GCC's `__extension__` marker; elsewhere it emits an ordinary static declaration with the platform's unmodified `PTHREAD_ONCE_INIT`. Applying `__extension__` only to the initializer expression is insufficient: the Hurd compiler still diagnoses the containing declaration. Do not weaken the job's warning policy or copy this workaround into individual translation units.

This small `__GNU__` branch is not evidence that GNU/Hurd is covered by Linux CI. Any additional use of `__GNU__`, `__gnu_hurd__`, or another reserved platform selector must enter the machine-readable macro inventory and this document in the same change. Prefer a capability probe unless the difference is genuinely an operating-system semantic that cannot be expressed that way.

Both Autotools and Meson builds are maintained. A change that passes on GNU/Linux can still fail on GNU/Hurd because the kernel, device model, process behavior, and available interfaces differ even when compiler and libc surfaces look similar.

The private [CLI terminal color transport](../../../converters/terminal-color-tty.c) writes short control sequences one byte at a time, checking readiness and the absolute deadline between bytes. On the maintained Hurd PTY, a larger write that only partly fits the output queue can block despite `O_NONBLOCK`; readiness alone does not make that larger write bounded. The [backpressure test](../../../tests/cli/terminal-color/0119_backpressure.t) verifies a partial timed write, a blocked request, and bounded restoration. Its fixture fills any remaining queue space explicitly because a slow PTY may reach the first deadline before filling the queue.

The transport also uses `pselect` with an explicit saved signal mask. Blocking cancellation signals across the flag check and atomically restoring that mask for the wait closes the check-to-wait race. In Hurd's implementation, the explicit mask additionally establishes a signal wakeup port before setting up the I/O RPCs. Preserve this atomic wait when maintaining the [CLI cancellation contract](../../encoder/output/output-colorspace.md#negotiation-through-the-proposed-color-protocol); a plain `select` after an unprotected cancellation check is not equivalent.

Hurd also immediately echoes queued raw input when canonical mode and echo are restored. Darwin defers retyping until another read, while Linux keeps the queued bytes without re-echoing them. The [TTY scripts](../../../tests/_support/cli/0038_cli_color_tty.c) disable echo in cases that intentionally leave unread input, while other cases continue to check restoration of an echo-enabled terminal. The flood fixture first sends a small payload exceeding the parser's header storage, so draining coverage does not depend on timer granularity. It then sends a byte after each short recovery-read timeout, sustaining input across the query deadline without filling the input queue and injecting a kernel-generated overflow BEL. Production cleanup still restores the original complete terminal mode and never flushes unread input.

## CI guest contract

The current Debian GNU/Hurd image is run as a single-vCPU amd64 guest with the Q35 machine model, an AHCI-backed disk exposed through the runner's IDE-compatible setting, and an e1000 network device. On the current CI host, the default `pc`/i440fx machine reached the root filesystem with repeated ext2 I/O errors, while Q35 booted the same image. A two-vCPU guest also lost the runner connection during a large source transfer; keep the job at one vCPU until the exact image and hypervisor combination has passed an explicit SMP stress run.

The image is prepared with a dedicated unprivileged runner account, offline SSH-key installation, and noninteractive sudo configuration. Console login behavior is not a substitute for SSH validation: an image that accepts an empty root password on its console can still reject that account through `sshd`. Image setup must therefore prove key-based login as the runner, required build tools, sudo, network access, clean shutdown, and a backing-file-free final image before the guest is scheduled.

With Python 3.13 and glibc 2.43, Meson's default asyncio child watcher has exposed a Hurd lock inversion between thread creation and signal delivery, leaving the test runner blocked even with no active test child. The Hurd job selects `asyncio.SafeChildWatcher` before executing the installed Meson entry point and retains serial test execution. This watcher handles child completion through the main event loop instead of creating a waiting thread for each child, while preserving subprocess output and exit status. The [Python 3.13 child-watcher documentation](https://docs.python.org/3.13/library/asyncio-policy.html#process-watchers) describes the distinction and the API's deprecation. Revalidate this startup policy when upgrading Python; an unavailable signal-watcher API must fail explicitly rather than silently restore the affected threaded path.

These machine and provisioning rules are runner requirements, not reasons to add source workarounds. When a job fails before checkout or configuration, diagnose the guest boot, storage, network, and SSH layers before changing libsixel.

## Maintenance checklist

- Keep GNU/Hurd separate from Debian GNU/Linux when assigning CI ownership; a Linux GCC job does not duplicate this platform/compiler pair.
- Run both the Autotools and Meson jobs after changing portability code, build discovery, shell helpers, or the test harness.
- Declare pthread once objects through `SIXEL_PTHREAD_ONCE_DECLARE(name)`; the platform staticcheck rejects direct source use of `PTHREAD_ONCE_INIT` and preserves the wrapper in both build-system source lists.
- Preserve Q35, the current disk and network device choices, and the single-vCPU limit until a replacement image and hypervisor pairing is demonstrated green.
- Revalidate runner SSH, sudo, compiler, build tools, and a complete source transfer whenever the base image is rebuilt.
- Add a source selector only when feature detection cannot express the required semantic, then extend the platform macro inventory and staticcheck assertions with the same change.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| HURD-01 | GNU/Hurd remains a distinct Tier 2 platform with both maintained build systems, its host triplet and capability-probe policy stay documented, and its `__GNU__` selector remains in the platform inventory. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |
| HURD-02 | Pthread once objects use the shared `SIXEL_PTHREAD_ONCE_DECLARE(name)` wrapper, the GNU/Hurd branch applies `__extension__` to the complete declaration, direct use of `PTHREAD_ONCE_INIT` stays absent outside the wrapper, and both build systems distribute the header. | [tests/_static/sh/staticcheck-platform-compat.sh](../../../tests/_static/sh/staticcheck-platform-compat.sh) |

### Coverage boundary

The static check preserves the public support row, policy link, host-triplet identity, reserved-macro audit, and pthread once compatibility seam. It cannot inspect the privately operated VM configuration or reproduce Hurd boot, storage, SMP, SSH, package, and Python child-watcher behavior; both live GNU/Hurd CI jobs remain required.
