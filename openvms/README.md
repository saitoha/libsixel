# OpenVMS build notes

This directory contains the first native OpenVMS build path for libsixel.
For the separate GNV/Autotools bring-up notes and future plan, see
`openvms/gnv-autotools.md`.

The current target is intentionally small:

- build a static OpenVMS object library, `libsixel.olb`
- link a bootstrap shareable image, `libsixelshr.exe`
- compile `smoke_sixel.c` against that library
- run the smoke program and produce `sixel_smoke.six`
- link and run `smoke_sixel_shared.exe` against `libsixelshr.exe`
- build `img2sixel.exe`
- convert `sixel_smoke.six` and produce `img2sixel_smoke.six`

Run from the repository root on OpenVMS:

```text
$ @ [.openvms]build.com
```

The current script is wired for the CI guest layout, where the repository is
expanded under the `SYSTEM` account's `SYS$SYSROOT:[SYSMGR]` view.

The build uses VSI C directly. It does not require GNV shell, Make, Meson, or
Autotools on the OpenVMS guest. The script generates `openvms/sixel.h` from
`include/sixel.h.in`, then compiles the source files listed in
`openvms/sources.dat`. The `img2sixel` build also compiles the converter
support files directly and uses the fallback `getopt_long()` implementation in
`converters/getopt_stub.h`.

This is a bootstrap port. The shareable image currently exports only the public
entry points required by `smoke_sixel.c`; the full public ABI still needs a
deliberate symbol-vector pass because several public API names exceed the
classic OpenVMS 31-character external symbol boundary. The current `img2sixel`
smoke path stays statically linked against `libsixel.olb` and uses the built-in
SIXEL loader, so it does not depend on libpng, libjpeg, WebP, or other optional
image libraries.
