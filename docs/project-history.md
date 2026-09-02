# Project History and Lineage

## In one sentence

libsixel is a descendant and code-lineage fork of KMIYA's `sixel` encoder and
decoder, expanded by Hayaki Saito and many contributors into a portable C
library, command-line toolset, image-processing pipeline, and integration
platform.

This is a statement about source lineage. libsixel was not created as a
Git-hosting fork that retained an upstream commit graph. The original source
was distributed as an archive and was imported into a separately created Git
repository.

## KMIYA's `sixel`

KMIYA's `sixel` was a compact C encoder and decoder distributed from
`nanno.dip.jp`. Its encoder departed from older printer-oriented encoders that
primarily minimized print-head movement. It organized output for efficient
transport to terminal emulators while retaining compatibility with SIXEL
devices. That approach became the starting point for libsixel's encoder.

The original archive is represented by the
[KMIYA `sixel` mirror](https://github.com/saitoha/sixel). The mirror records
KMIYA's original licensing statement and identifies the project as the origin
of libsixel. Current derived source files, including
[`src/fromsixel.c`](../src/fromsixel.c), also retain explicit attribution and
describe their relationship to the 2014-03-02 original version.

## Formation of libsixel

The repository began with an initial commit on 2013-08-20. The recognizable
libsixel development line started in March 2014:

- On 2014-03-19, commit
  [`0ebd7dd3c`](https://github.com/saitoha/libsixel/commit/0ebd7dd3c62353eeb0bbfc0dd2a8ee26c71f4777)
  imported KMIYA's `tosixel.c` and `fromsixel.c` from the distributed archive.
- The imported decoder was removed the next day while the new project began
  separating and rebuilding its library interfaces.
- A snapshot identified as the 2014-03-02 original was committed on 2014-03-21
  for provenance. It was later moved to the standalone mirror rather than kept
  as a second implementation inside libsixel.
- The early project removed the encoder's dependency on GD, introduced public
  headers, added the `img2sixel` and `sixel2png` programs, and adopted
  Autotools and Automake.
- The first package release, v0.11.0, was tagged on 2014-04-30. The project
  reached v1.0.0 on 2014-08-17.

The project is distributed under the [MIT license](../LICENSE). Files derived
from earlier work retain their attribution notices. Licensing history should
be read from the applicable source header and license file rather than inferred
only from the repository's current top-level license.

## Development history

This is a short history of durable changes in direction, not a release-by-
release changelog.

### 2014: from utility to reusable library

The first year established most of the project's identity. The code moved into
library and converter directories, gained public encoding and decoding APIs,
added color quantization, multiple diffusion and resampling methods, resizing,
animation handling, 7-bit and 8-bit output, and broad Unix and Windows build
support.

Community work was important from the beginning. Araki Ken (`@arakiken`)
contributed or designed high-compression output, 15-bit high-color operation,
clipping, and GNU Screen integration. Haru (`@uobikiemukot`) contributed
support for multiple pixel formats and helped shape the DCS-envelope API.
Discussions and patches from terminal, operating-system, and packaging
developers repeatedly turned terminal-specific behavior into reusable library
contracts.

### 2015-2017: API and ecosystem expansion

The library gained higher-level encoder, decoder, frame, allocator, image-load,
and image-write interfaces. Transparency and multi-frame image handling became
first-class concerns. Python, Perl, PHP, and Ruby bindings and the
`libsixel-config` helper broadened the integration surface. A new canonical
decoder in the 1.7 series focused on predictable and safe decoding.

### 2018-2020: security and distribution hardening

Reports from security researchers and users exposed integer overflows, buffer
errors, invalid state transitions, excessive allocation, hangs, leaks, and
malformed-image handling problems. Fixes across the 1.8 series made adversarial
input a central part of the decoder and loader contract. Work in the same
period improved VPATH builds, packaging, dependency handling, and immutable
release artifacts.

### 2021-2025: build-system and platform renewal

Meson support and GitHub Actions were added alongside the established
Autotools build. Python bindings moved to Python 3 packaging. Later work
renewed Windows support, optional native loaders, generated build inputs, and
cross-platform CI coverage while preserving compatibility with existing
library and command-line users.

### 2026 and the current development line

Recent development has broadened the project beyond its historical encoder
core: structure-aware fuzzing, stronger static repository checks, built-in
image decoders, explicit component boundaries, parallel processing, measured
image-quality policy, GPU-assisted paths, and a much larger portability matrix.
The central obligation remains unchanged: new implementation strategies must
continue to produce and consume interoperable SIXEL streams.

## How contributions shaped the project

libsixel is maintainer-led but has never been a single-source project.
Contributions include code, testing, specifications, portability reports,
security disclosures, packaging, performance measurements, and design
discussion. Some of the most consequential work began as a bug report or
terminal-compatibility observation rather than a large patch.

The project records credit in several complementary places:

- [`AUTHORS`](../AUTHORS) is the curated contributor list.
- [`NEWS`](../NEWS) records release landmarks and thanks contributors and
  reporters for specific changes.
- [`ChangeLog`](../ChangeLog) and the Git history preserve detailed development
  chronology.
- Issue and pull-request discussions preserve design context that a commit
  author line cannot capture.

No single one of these is a complete measure of contribution. In particular,
commit counts undercount review, testing, research, security reporting, and
work that was imported or co-developed outside GitHub. When adding historical
claims or credits, verify them against the original commit, release note, or
discussion and update the durable record where appropriate.

## Further reading

- [KMIYA `sixel` source mirror](https://github.com/saitoha/sixel)
- [libsixel release history](../NEWS)
- [libsixel contributors](../AUTHORS)
- [SIXEL format](sixel-format.md)
- [Functional overview](functionality/overview.md)
