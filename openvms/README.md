# OpenVMS build notes

This directory contains the first native OpenVMS build path for libsixel.

The current target is intentionally small:

- build a static OpenVMS object library, `libsixel.olb`
- compile `smoke_sixel.c` against that library
- run the smoke program and produce `sixel_smoke.six`

Run from the repository root on OpenVMS:

```text
$ @ [.openvms]build.com
```

The current script is wired for the CI guest layout, where the repository is
expanded under `SYS$SPECIFIC:[SYSMGR]` for the `SYSTEM` account.

The build uses VSI C directly. It does not require GNV shell, Make, Meson, or
Autotools on the OpenVMS guest. The script generates `openvms/sixel.h` from
`include/sixel.h.in`, then compiles the source files listed in
`openvms/sources.dat`.

This is a bootstrap port. Shared image support, CLI programs, and the full
test suite are intentionally left for later steps.
