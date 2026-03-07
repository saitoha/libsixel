# Security Policy

## Reporting a Vulnerability

If you believe you have found a security vulnerability in **libsixel**, please use GitHub's **Private Vulnerability Reporting** feature.

This allows maintainers to review and fix the issue before public disclosure.

Please do **not** open a public issue for vulnerabilities that may allow memory corruption or code execution.

Examples include:

* heap buffer overflow
* stack buffer overflow
* use-after-free
* out-of-bounds write
* out-of-bounds read that may lead to information disclosure
* integer overflow leading to memory corruption
* arbitrary code execution

When reporting a vulnerability, please include a **proof-of-concept input file** if possible.

---

## Issues that should be reported publicly

The following issues are **not treated as security vulnerabilities** and should be reported as normal GitHub issues:

* denial of service (DoS)
* excessive memory allocation / out-of-memory
* memory leaks
* infinite loops
* performance problems
* malformed image files causing decoding failures

Please open a standard GitHub issue for these cases.

---

## Forked and archived repositories

Security issues affecting the archived upstream fork **libsixel/libsixel** may also be reported here.

However, fixes can only be provided if the issue can be reproduced in this repository (**saitoha/libsixel**).

If the issue only affects older forked versions and cannot be reproduced here, the maintainers of this repository may not be able to provide a fix. In such cases, downstream distributions or package maintainers that ship the affected fork may handle the issue independently.

---

## Disclosure policy

For confirmed security vulnerabilities:

1. A fix will be prepared.
2. The fix will be released as soon as possible.
3. The issue may be disclosed publicly once a patch is available.

The libsixel project does not manage CVE assignments directly.
Downstream distributions or external security databases may assign CVE identifiers independently.

---

## Scope

This policy applies to:

* the **libsixel library**
* bundled utilities such as `img2sixel`
* image decoding and encoding functionality

Issues in third-party dependencies should be reported to their respective upstream projects.

---

## Supported versions

Security fixes are generally applied to:

* the latest development branch
* the most recent release

Older versions may not receive security updates.

---

## Reporting guidelines

To help maintainers reproduce and fix issues efficiently, please include as much of the following information as possible in your report:

* the **input file** that triggers the issue (minimal proof-of-concept if possible)
* the **exact command line** used
* the **libsixel version or commit hash**
* the **build configuration** (compiler, sanitizer, build flags)
* the **platform and operating system**
* the **crash log or stack trace**

Example:

```
command:
img2sixel poc.png

version:
libsixel commit 1a2b3c4

build:
clang 17
AddressSanitizer enabled

platform:
Ubuntu 24.04 x86_64

stack trace:
<ASan output here>
```

If the issue was discovered by fuzzing tools (such as libFuzzer, AFL++, or OSS-Fuzz), please include the fuzzing output and the minimized test case.

---

## Minimizing test cases

When possible, please try to provide a **minimized input file** that still reproduces the issue.

Large fuzzing inputs can make debugging more difficult and slow down analysis.

Helpful techniques include:

* reducing the input file size while preserving the crash
* removing unnecessary metadata or chunks
* providing the smallest input that still reproduces the issue

If the issue was found by a fuzzing tool, consider using the minimization tools provided by the fuzzer:

* `afl-tmin`
* `libFuzzer -minimize_crash`
* `honggfuzz --minimize`

Providing a minimized test case greatly helps maintainers analyze and fix the issue more quickly.

---

## Proof-of-concept test cases

By submitting a vulnerability report, you agree that any provided proof-of-concept files (PoC inputs) may be used by the libsixel project for testing purposes.

This may include:

* adding the PoC to the test suite
* using the PoC as a regression test
* modifying the PoC to create minimized test cases

These files may be stored in the repository and distributed as part of the test suite.

If you do not want your PoC to be included in the repository, please clearly indicate this in your report.

Please avoid submitting proof-of-concept files that contain copyrighted third-party content.

Prefer minimized or synthetic inputs created specifically to reproduce the issue.
