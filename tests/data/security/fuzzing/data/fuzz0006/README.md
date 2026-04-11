# fuzz0006 Regenerated JPEG PoC

This input is generated locally from JPEG syntax primitives.
It is not copied from external corpora.

## Purpose

The file exercises the historical `stb_image` JPEG IDCT signed-overflow path
that was fixed by widening intermediate accumulators.

## Regeneration

Run the generator from the repository root:

```sh
./tools/regenerate-fuzz0006-jpeg-idct-overflow.py
```

Default output path:

- `tests/data/security/fuzzing/data/fuzz0006/jpeg_idct_overflow_regenerated.jpg`
