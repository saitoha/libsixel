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

Two different fork relationships appear in the project's history:

- libsixel itself is a code-lineage fork of KMIYA's archive, not a GitHub
  fork;
- [`libsixel/libsixel`](https://github.com/libsixel/libsixel) is a GitHub fork
  of [`saitoha/libsixel`](https://github.com/saitoha/libsixel), created in 2021
  after updates by @saitoha had been stalled for more than a year.

The second relationship was not a planned division of maintenance or a
transfer of the original repository. It was a community continuation created
in response to the update gap. Because it supplied later releases while the
original repository was inactive, many distributions and users came to treat
`libsixel/libsixel` as the practical successor and upstream.

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

The March 2014 import was a branch point, not the end of development in
KMIYA's line. KMIYA was also the developer of the Windows terminal emulator
[RLogin](https://github.com/kmiya-culti/RLogin), and the standalone `sixel`
utility continued to change alongside RLogin's SIXEL implementation:

- The standalone
  [`v20141107-tc`](https://github.com/saitoha/sixel/tree/v20141107-tc)
  source introduced its true-color variant only months after libsixel
  branched. RLogin 2.17.2 accepted 0-255 RGB components, and its
  [2.17.3 history](https://github.com/kmiya-culti/RLogin/blob/master/docs/history.html)
  records SIXEL extensions for indexed selection and 24-bit color. The
  [`v20141206`](https://github.com/saitoha/sixel/tree/v20141206) standalone
  version then merged the true-color extension into KMIYA's main utility.
- The later
  [`v20180723`](https://github.com/saitoha/sixel/tree/v20180723) source merged
  an RGBA extension. It added an 8-bit-per-channel RGB color space and an RGBA
  form, including an alpha component and declared per-channel maxima. The
  corresponding RLogin 2.23.7 release extended DECGCI color resolution and
  transparency; RLogin's
  [control-sequence reference](https://github.com/kmiya-culti/RLogin/blob/master/docs/ctrlcode.html)
  documents both the 0-255 RGB and alpha extensions.

The mirror commit that imports `v20180723` is dated 2025, but the version name
and matching RLogin 2.23.7 history place that source development in 2018. The
standalone releases should therefore be understood as a parallel, actively
evolving KMIYA line rather than as frozen snapshots of the code from which
libsixel branched.

The two lines also came to emphasize different goals. KMIYA's true-color work
expanded the standalone encoder's palette table from 256 to 1,024 entries and
developed private extensions together with RLogin. libsixel concentrated on a
portable library and CLI, color quantization, and compatibility across the
wider terminal ecosystem; its generated palettes remain limited to 256 entries
by [`SIXEL_PALETTE_MAX`](../include/sixel.h.in). It did not import most of the
KMIYA/RLogin-specific extension series. This divergence should not be mistaken
for inactivity in either line.

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

### 2020-2021: stalled updates and the community fork

The last @saitoha-led merge before the gap was made in January 2020. Other
contributors continued to prepare fixes, but updates to the original
repository stalled. That created practical problems for security fixes,
packagers, pending pull requests, and downstream projects that needed a
maintained release.

On 2021-06-09, Fredrick R. Brennan (`@ctrlcctrlv`) announced the
[`libsixel/libsixel`](https://github.com/libsixel/libsixel) fork in
[`saitoha/libsixel` issue 154](https://github.com/saitoha/libsixel/issues/154).
The new `libsixel` GitHub organization presented the fork as a community
continuation while @saitoha was inactive. It imported pending contributions,
addressed security issues, and aimed to provide responsive review and releases.
The original `saitoha/libsixel` repository was neither renamed nor transferred.

### 2021-2025: work in `libsixel/libsixel`

The community fork developed its own release line. Its contributors added
security and lifetime fixes, GitHub Actions, Meson support, Python 3 packaging,
dependency and distribution improvements, and other maintenance work. It
published v1.9.0 and the v1.10 series through v1.10.5. Fredrick R. Brennan,
nick black (`@dankamongmen`), Eli Schwartz, Henner Zeller, WSLUser, and other
contributors participated at different points in that work.

This period must not be collapsed into inactivity in the wider project
history: even though updates in `saitoha/libsixel` had stalled, substantial
maintenance and release work continued in `libsixel/libsixel`. The fork became
the de facto successor for much of the packaged ecosystem. Through 2026,
[Debian stable](https://packages.debian.org/stable/source/libsixel),
[Arch Linux](https://archlinux.org/packages/extra/x86_64/libsixel/), and
[Fedora 43](https://packages.fedoraproject.org/pkgs/libsixel/libsixel/fedora-43.html)
continued to ship the fork's v1.10.5 line; Arch also identified
`libsixel/libsixel` as its upstream URL.

That downstream status does not erase the original repository or make the two
Git histories interchangeable. The fork's issues, pull requests, releases,
and commits are a distinct and essential part of the project's development
record. Because both lines use the same project and library names, package and
security records should identify the repository and commit or tag explicitly;
a version number or issue number alone can be ambiguous.

### 2025: development resumes in `saitoha/libsixel`

Active development resumed in the original repository in 2025. The two
repositories had accumulated different commits and release histories, so the
result was not a simple rename or fast-forward from the community fork. Work
from the fork has been incorporated or reimplemented where appropriate, and
the current repository still refers to `libsixel/libsixel` issues and commits
when preserving that provenance.

That reuse includes direct cherry-picks of security work from the community
fork. In August 2025, the original line imported the fork's fixes for
[CVE-2020-11721](https://github.com/saitoha/libsixel/commit/76b491d7c47a0718734a46c989a9674668177776),
[CVE-2020-19668](https://github.com/saitoha/libsixel/commit/f39d6dafe8950bcb30848c3a996d4f67b4ba6dc6),
and
[CVE-2021-45340](https://github.com/saitoha/libsixel/commit/1c58a6ea708b6fa793ffb5a10798ccfea36e8eed).
They retain their original authorship and patches while recording their later
integration into `saitoha/libsixel`. Other fork work has been evaluated and
integrated or reimplemented individually rather than by merging the two
histories wholesale.

The repository documented here is the resumed `saitoha/libsixel` development
line. This statement identifies the source tree, not an exclusive claim to the
name in downstream packaging. `libsixel/libsixel`, which GitHub archived on
2025-02-12, remains the source of the v1.10 line used by many distributions and
an important source of fixes, releases, discussion, and contributor credit.

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
- Issue and pull-request discussions in both `saitoha/libsixel` and
  `libsixel/libsixel` preserve design context that a commit author line cannot
  capture.

No single one of these is a complete measure of contribution. In particular,
commit counts undercount review, testing, research, security reporting, and
work that was imported or co-developed outside GitHub. When adding historical
claims or credits, verify them against the original commit, release note, or
discussion and update the durable record where appropriate.

## Further reading

- [KMIYA `sixel` source mirror](https://github.com/saitoha/sixel)
- [`libsixel/libsixel` community fork](https://github.com/libsixel/libsixel)
- [Community-fork announcement and rationale](https://github.com/saitoha/libsixel/issues/154)
- [libsixel release history](../NEWS)
- [libsixel contributors](../AUTHORS)
- [SIXEL format](sixel-format.md)
- [Functional overview](functionality/overview.md)
