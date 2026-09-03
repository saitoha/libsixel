# `auto` Lookup Policy

## Purpose

`auto` asks the lookup-policy selector to choose a concrete backend. It does
not define a distance, index, or complexity class of its own. The selected
backend owns all of those properties.

The built-in encoder default is currently `certlut`. Therefore an explicit
`--lookup-policy=auto` is not merely another spelling of the default.

## Dispatch flow

The current selector behaves as follows:

```text
completed palette and pixel format
              |
              v
 canonical two-color black/white palette? -- yes --> internal mono policy
              |
              no
              v
 lookup disabled, depth != 3, or explicit none? -- yes --> none
              |
              no
              v
 recognized concrete policy? -- yes --> that 8-bit or float32 backend
              |
              no
              v
             6bit
```

`auto` reaches the final branch and currently resolves to `6bit`, selecting
the 8-bit or float32 implementation from the pixel representation. The float32
`6bit` backend is an exhaustive normalized-distance scan rather than a dense
RGB666 table.

## Guarantee and cost

`auto` inherits the selected backend's result guarantee, preparation cost,
query cost, and storage. Its own dispatch is constant work.

Benchmarks must report the resolved class name rather than only saying
"auto." Otherwise a selector change can make two results incomparable without
changing the benchmark command.

## Name and source

`auto` is a libsixel descriptive name, not the name of a published search
algorithm. It means deferred policy selection.

The selection rules are implemented in
[`lookup-policy.c`](../../../src/lookup-policy.c). The public values are
registered in [`options-registry.c`](../../../src/options-registry.c).

Return to the [Lookup Policy index](../lookup-policy.md).
