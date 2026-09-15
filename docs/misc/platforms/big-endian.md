# Big-endian compatibility

## Scope

Big-endian compatibility is an architecture and object-representation boundary rather than an operating-system selector. The maintained CI evidence comes from Linux/s390x and NetBSD/SPARC64, while little-endian targets include the more common x86_64, AArch64, PPC64LE, RISC-V 64, and MIPS64LE configurations. Current support status and exact build combinations belong in [Build, Runtime, and Platform Support](../../platform-support.md) and the owning workflow.

## Word-sized byte comparisons

The encoder accelerates repeated-byte searches in `sixel_emit_span_from_map()` by copying a chunk into an `unsigned long`, XORing it with a word containing the expected SIXEL byte, and then locating the first nonzero byte. The chunk was copied from increasing memory addresses, so the mismatch scan must inspect the object representation in that same address order.

Integer significance is not memory order. Repeatedly testing the low eight bits and shifting the word right visits increasing addresses only on little-endian machines; on a big-endian target it starts at the opposite end of the copied chunk. That changes the computed run length and can make encoded pixels extend into unrelated columns even though the word-level equality check is correct.

After `memcpy`, inspect the word through an `unsigned char const *` and advance the byte index from zero through the copied length. C permits character types to inspect an object's representation, and `memcpy` avoids alignment and aliasing assumptions at the source address. Do not replace this address-order scan with numeric shifts unless the implementation explicitly translates between byte order and address order and has equivalent big-endian regression coverage.

## CI and maintenance

The Linux/s390x and NetBSD/SPARC64 jobs run the complete registered Autotools and Meson suites. SPARC64 uses full-system emulation and therefore divides each build system's registered tests into eight disjoint shards; the job family, not an individual shard, is the unit of complete-suite evidence. A platform can be called supported only when every shard is green.

When adding another word-at-a-time optimization, audit both the all-equal fast path and the position of the first mismatch. A little-endian unit test cannot establish memory-order independence by itself, so retain the structural static check and the live big-endian CI jobs together.

## Test coverage

<!-- test-coverage: enforced -->

| ID | Contract | Owning test |
| --- | --- | --- |
| BE-01 | The encoder locates a mismatch by inspecting the copied word in increasing memory-address order and does not recover positions by shifting toward the least-significant byte. | [tests/_static/sh/staticcheck-big-endian-compat.sh](../../../tests/_static/sh/staticcheck-big-endian-compat.sh) |
| BE-02 | Linux/s390x and NetBSD/SPARC64 retain both build systems and collectively run every registered test, with SPARC shards covering the full suite without smoke-test substitution. | [tests/_static/sh/staticcheck-big-endian-compat.sh](../../../tests/_static/sh/staticcheck-big-endian-compat.sh) |

### Coverage boundary

The structural check protects the address-order implementation but cannot execute the code under a different object representation. GitHub Actions provides that runtime evidence; transient infrastructure failures must remain distinguishable from deterministic encoding or test failures.
