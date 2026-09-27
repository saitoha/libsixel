# Terminal Graphics Color Management Proposal

Status: experimental protocol proposal, revised 2026-09-27. This document proposes control sequences and color semantics for applications and terminals exchanging SIXEL graphics. It does not assert adoption by existing terminals or registration of the proposed selectors. Support must be discovered from the receiving session, not inferred from a terminal name, operating system, or display model.

The proposal separates the interpretation of incoming RGB numbers from the terminal's conversion to an actual display. Applications can send sRGB, convert their source colors into a reported input RGB definition, or request a defined gamut expansion. Ordinary SIXEL RGB percentages contain none of this metadata; see [SIXEL color registers](../sixel-format.md#graphics-color-introducer). The extension supplies the missing agreement outside the image stream.

The [output color-space guide](../functionality/output-colorspace.md) describes the implemented `img2sixel -U` conversions. The selected automatic negotiation path is implemented in the CLI; the library does not perform this negotiation. The protocol below also describes simple RGB and ICC definitions that exceed the current selected Display P3 encoder path.

## Scope and notation

Two independently addressed targets are defined. Text colors and text palette definitions are outside this extension, as are monochrome DRCS glyphs that obtain their colors from text attributes.

| Target `T` | Meaning |
| --- | --- |
| `1` | Ordinary SIXEL graphics. |
| `2` | Colored DRCS-SIXEL glyph definitions. |

Every operation supplies its target. There is no selected-target state, target mask, or implicit target remembered from an earlier request. Target identifiers, color-definition identifiers, and other enumerations have independent allocation spaces.

The examples use `CSI = ESC [` and `DCS = ESC P`, terminated where shown by `ST = ESC` followed by a backslash. `ESC` is `0x1b`. Senders use these 7-bit forms; receivers also recognize their C1 equivalents (`0x9b`, `0x90`, and `0x9c`) where the transport delivers C1 controls. Explanatory spaces, angle brackets, and brackets denoting optional fields are not transmitted.

The header follows the DEC control-sequence byte classes: parameter bytes `0x30..0x3f`, intermediate bytes `0x20..0x2f`, and a final byte `0x40..0x7e`. The selector `./p` contains two intermediate bytes, `.` and `/`, followed by final byte `p`; `./r` ends in `r`. These intermediate bytes are in the stated order. ICC data follows the DCS final byte, never inside its parameters. The [xterm control-sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html) describes the existing CSI/DCS framing and DECRQSS mechanism; the new selectors and fields here are this proposal's additions.

Numeric fields are unsigned ASCII decimal integers in `0..9998`. The value `9999` is reserved and invalid in every field, including unknown target and definition identifiers: the [DEC parameter rules](https://www.zx.net.nz/documentation/mds-199909/cd3/term/vt510rmb.pdf#page=92) saturate larger values to `9999`, so accepting it would make a valid field indistinguishable from a clipped value. Receivers reject both literal `9999` and larger values, whether they inspect the original digits or receive saturated parameters. This rule also applies to the numeric fields inside the DECRQSS body. Signs, colon subparameters, unexpected private prefixes, and overflow are invalid. Larger logical values use the explicitly defined base-9999 limbs below; the per-parameter limit does not apply to binary data in the ICC payload. Only fields explicitly documented below have defaults. Receivers wait for the complete sequence before applying it. CAN or SUB cancels an incomplete sequence; an ESC that does not complete ST abandons the current string and starts escape parsing. Read boundaries do not terminate or restart a sequence.

## Set display color policy

```text
CSI T;M[;P3]./p
```

Parameter 1 is target `T`, parameter 2 is policy `M`, and parameter 3 is reserved. The spelling `P3` in this grammar means the third parameter, not Display P3.

| Policy `M` | Interpretation of newly defined image RGB |
| --- | --- |
| Omitted, empty, or `0` | Default sRGB interpretation; retain the stored policy value `0`. |
| `1` | Explicit sRGB interpretation; retain the stored policy value `1`. |
| `2` | The device-input RGB definition reported for this target. The application supplies values converted to that definition. |
| `3` | Interpret the input as sRGB and expand its linear component values into the reported device-input RGB basis, as defined below. |

Target `T` is mandatory and must be `1` or `2`. An absent, empty, zero, or unknown target makes the entire setting invalid. Reserved `P3` accepts only omission, an empty field, or `0`. Unknown policies, unsupported policies, nonzero `P3`, four or more parameters, or malformed syntax leave both targets unchanged. A setting has no direct reply. Senders use the canonical form `CSI T;M./p`.

`CSI 1./p`, `CSI 1;./p`, and `CSI 1;0;0./p` all set the SIXEL policy to `0`. `CSI 1;2./p` sets only SIXEL to device-input RGB; it does not change DRCS-SIXEL or text colors.

### Query and restore with DECRQSS

```text
request: DCS $qT./p ST
success: DCS 1$rT;M./p ST
failure: DCS 0$r ST
```

The request string contains one target followed by `./p`. It contains neither a policy nor the reserved parameter. An absent, zero, unknown, unavailable, or malformed target, or any extra parameter, produces the failure response. The successful response reports the current stored policy and omits the reserved parameter. Prefixing its `T;M./p` body with CSI restores that target's setting.

For example, `DCS 1$r1;0./p ST` reports SIXEL's default sRGB policy. It is not a device capability report. Although policies `0` and `1` currently have the same color interpretation, save and restore their numeric values exactly. Querying never changes the setting.

There is no application-generated request identifier. Because a failed DECRQSS response has no target, applications serialize policy queries and coordinate them with other DECRQSS users on the same input stream. Device color-space details use the separate exchange below, not DECRQSS.

## Query device-input RGB

```text
request: CSI ?T./r
reply:   DCS T;A;S2;S3;C[;Gh;Gm;Gl[;P[;W]]]./r <payload> ST
```

The request has exactly one explicit numeric target, preceded by `?`. Other prefixes, an omitted or empty target, extra parameters, subparameters, and overflow receive no reply. A syntactically valid but unknown target, including `0`, receives an unavailable response echoing that target. The reply has no `?` prefix.

| Field | Meaning |
| --- | --- |
| `T` | The exact target requested. |
| `A` | `1` when the target's information is available; otherwise `0`. Availability also guarantees policies `0` and `1`. |
| `S2` | `1` if policy `2` is supported throughout reception, retained storage, and rendering; otherwise `0`. |
| `S3` | `1` if policy `3` is supported throughout that path; otherwise `0`. |
| `C` | Color definition, using the allocation below. |
| `Gh`, `Gm`, `Gl`, `P`, `W` | Additional fields allowed only for the simple RGB definition, `C=1`. |
| Payload | Complete encoded ICC data, required only for `C=2`. |

`A`, `S2`, and `S3` are separate Boolean fields, each explicitly `0` or `1`, not bits packed into a mask. If `A=0`, both support fields must be `0`. Whenever both support fields are `0`, the color definition is sRGB (`C=3`), without extra fields or payload. Thus `1;1;0;0;3./r` reports an available sRGB-only target, while `1;0;0;0;3./r` reports unavailable information. Neither permits assuming device-input support.

Replies contain capabilities and a fixed RGB definition, not the current policy. They do not carry request identifiers, transfer budgets, fragment numbers, or decoded-length fields. Replies are returned in request order; applications allow at most one unresolved device-definition query per target. There are no unsolicited profile replies or profile-change notifications.

### Color-definition allocation

| `C` | Definition or reservation |
| --- | --- |
| Omitted, empty, or `0` | sRGB, identical to `3`. |
| `1` | Simple RGB: a shared power transfer curve, named primaries, and a named white point. |
| `2` | A complete ICC RGB display profile in the DCS payload. |
| `3` | sRGB: sRGB primaries, D65, and the piecewise sRGB transfer curve. |
| `4` | Display P3: P3 primaries, D65, and the piecewise sRGB transfer curve. |
| `5` | Adobe RGB (1998), including its defined transfer exponent. |
| `6..9` | Reserved. |
| `10` | Reserved for DCI-P3. |
| `11` | Reserved for ACEScg Linear. |
| `12` | Reserved for BT.2020 RGB. |
| `13` | Reserved for BT.709 RGB. |
| `14` | Reserved for ROMM RGB. |
| `15..9998` | Available for future explicitly specified definitions. |

The reservations at `10..14` do not yet define usable wire color spaces: transfer functions, luminance conventions, and other required details must be specified before advertising them. Parsers must support multi-digit identifiers and must not reinterpret an unknown definition as sRGB.

Named definitions are exact contracts, not approximations to a monitor. In particular, `C=4` is neither DCI-P3 nor P3 primaries with a simple gamma 2.2 curve. The primary and transfer definitions for sRGB and Display P3 are given in [CSS Color 4](https://www.w3.org/TR/css-color-4/#predefined-display-p3). Adobe RGB (1998) uses the exponent `563/256`, as specified by the [ICC RGB registry](https://registry.color.org/rgb-registry/adobergb).

Common-name responses have four fields when `C` is omitted, or five when present, and no payload. `C=2` requires five fields and a nonempty ICC payload. `C=1` requires eight to ten fields and no payload. Additional fields, including empty fields, are invalid for a known format that does not define them. An application that does not implement a reported color definition must finish consuming the response without activating that transform.

### Simple RGB definition

```text
DCS T;A;S2;S3;1;Gh;Gm;Gl[;P[;W]]./r ST
```

This describes relative SDR RGB with black at linear `(0,0,0)` and white at linear `(1,1,1)`, normalized to white luminance `Y=1`. Each SIXEL channel percentage yields the encoded component `u = percentage / 100`. All three components use the same positive exponent:

```text
G = (Gh * 9999 + Gm) * 9999 + Gl
gamma = G / 100000
linear = encoded ^ gamma
encoded = linear ^ (1 / gamma)
```

`Gh`, `Gm`, and `Gl` are mandatory, nonempty parameters in `0..9998`, sent high limb first. Zero is a literal limb value, not a default. Reconstruct the logical integer `G` and require `1..2147483647`; the all-zero triple and any larger reconstructed value are invalid. Validate before multiplication so reconstruction cannot overflow a 32-bit implementation. The maximum valid triple is `21;4790;8416`; `21;4790;8417` is already out of range. No parameter contains the unsplit `G`.

A sender first computes `G = floor(gamma * 100000 + 0.5)`, validates the logical range, and divides it into limbs:

```text
Gh = G / 99980001
Gm = (G / 9999) % 9999
Gl = G % 9999
```

These divisions are integer divisions. The terminal uses the reconstructed integer's exponent as its input definition, not a hidden unrounded exponent. This preserves the exponent precision without placing a value at the scanner's saturation point on the wire.

| Logical `G` | `Gh;Gm;Gl` | Meaning |
| --- | --- | --- |
| `1` | `0;0;1` | Smallest positive exponent, 0.00001. |
| `100000` | `0;10;10` | Linear. |
| `220000` | `0;22;22` | Gamma 2.2. |
| `260000` | `0;26;26` | Gamma 2.6. |
| `219998` | `0;22;20` | Approximation from PNG gAMA=45455. |
| `2147483647` | `21;4790;8416` | Largest logical value. |

For example, `DCS 1;1;1;1;1;0;22;22./r ST` reports gamma 2.2 with the default primaries and white. `DCS 1;1;1;1;1;0;26;26;2;3./r ST` reports P3 primaries, DCI white and a simple gamma 2.6 curve. This grammar has exactly three gamma limbs; receivers do not infer a different layout from their values or attempt an unsplit-gamma compatibility form.

`P` selects primaries only; `W` selects white independently. Omitted, empty, or zero `P` and `W` each mean `1`. These defaults never depend on previous queries, a physical display, or another color definition. Unknown values are unusable; they are not rounded or replaced by a nearby definition.

| `P` | Primaries | Red xy | Green xy | Blue xy |
| --- | --- | --- | --- | --- |
| `1` | sRGB / BT.709 | `(0.6400, 0.3300)` | `(0.3000, 0.6000)` | `(0.1500, 0.0600)` |
| `2` | P3 | `(0.6800, 0.3200)` | `(0.2650, 0.6900)` | `(0.1500, 0.0600)` |
| `3` | Adobe RGB (1998) | `(0.6400, 0.3300)` | `(0.2100, 0.7100)` | `(0.1500, 0.0600)` |

| `W` | White | xy |
| --- | --- | --- |
| `1` | D65 | `(0.3127, 0.3290)` |
| `2` | D50 | `(0.3457, 0.3585)` |
| `3` | DCI white | `(0.3140, 0.3510)` |

The decimal coordinates in these tables are authoritative for this simple format. `P` and `W` are independent enumerations; values above `3` remain undefined. P3 primaries alone do not specify Display P3. The [ICC DCI-P3 registry](https://registry.color.org/rgb-registry/dcip3) identifies the DCI white and power curve; absolute cinema luminance and viewing conditions are not encoded here.

For each primary `(x,y)`, form a column `(x/y, 1, (1-x-y)/y)` and collect the columns in `V`. Form the corresponding white vector `w`, solve `s = inverse(V) * w`, and define `M = V * diag(s)`. Then `XYZ = M * linear_RGB` and `M * (1,1,1) = w`. Changing `W` does not silently replace or adapt the primary coordinates. When connecting this definition to another reference white, the reference transform uses full Bradford chromatic adaptation; see the [RGB/XYZ conversion examples](https://www.w3.org/TR/css-color-4/#color-conversion-code). Preserve extended intermediate values when converting through a working RGB space; output gamut mapping is a separate operation.

The exponent direction differs from [PNG gAMA](https://www.w3.org/TR/png-3/#11gAMA). For positive PNG integer `g`, the corresponding approximation is `G = floor(10000000000 / g + 0.5)`, evaluated with sufficient arithmetic width. For example, `g=45455` gives `G=219998`, transmitted as `0;22;20`, not `45455` or exactly `220000`. A simple power curve cannot exactly replace the piecewise sRGB/Display P3 curve or the Adobe RGB exponent at this integer precision. Use the named definition or ICC for those exact contracts.

### ICC definition and 3-in-4 transport

`C=2` carries one complete ICC v2 or v4 RGB display profile. Its transforms must permit the application to convert source colors to the reported RGB definition and the terminal to render those RGB values. Gray, CMYK, DeviceLink, abstract, named-color, iccMAX, and HDR negotiation are outside this proposal. Reading the profile does not authorize changing display calibration or installing an OS profile.

The sender prepares the entire profile before starting the reply and sends it in one DCS. It must not report an ICC definition successfully with an absent or partial profile. There is no compression, fragmentation, profile-request variant, or separate decoded-length field. A receiver may enforce a resource limit, but must reject the whole definition if it cannot receive and validate it. If the input remains live, it drains the rest of the rejected DCS through its terminator without allocating for the discarded payload.

The payload is an MSB-first stream of six-bit values. Add `0x3f` to each value to obtain one byte in `0x3f..0x7e`. This avoids DEL and all control bytes. There is no Base64 alphabet lookup or padding character; the transport has the same four-for-three size ratio for complete groups.

```text
input bytes: a, b, c
sextets:     a >> 2
             ((a & 3) << 4) | (b >> 4)
             ((b & 15) << 2) | (c >> 6)
             c & 63
wire byte:   sextet + 0x3f
```

Three input bytes produce four payload bytes. A final one-byte input produces two bytes; a final two-byte input produces three. Unused low bits of the last sextet must be zero. A payload length congruent to `1` modulo `4` is invalid. DEL, spaces, line breaks, and bytes outside the alphabet are invalid payload data. For example, input `00` encodes as `??`, and input `ff` as `~o`.

Only a complete, normally terminated DCS may activate a new color definition. Check the encoding and tail bits, decoded length against the profile's own size field, tag bounds, RGB/profile kind, and acceptance of the required transforms by the CMS. ICC defines its own 32-bit profile-size field; it is not a protocol allocation request. Profile size and validation requirements are described in [ICC.1:2022](https://www.color.org/specification/ICC.1-2022-05.pdf), sections 7.2.2 and 8. Neither a partially accepted ICC nor malformed replacement may discard an already valid transform or change previously stored colors.

Small matrix profiles do not establish a universal small size limit. A single RGB lookup table with 65 points per axis, three output components, and 16 bits per component needs `65^3 * 3 * 2 = 1647750` bytes before its surrounding profile structures. Implementations choose resource budgets independently of the wire format and consume unsupported definitions without pretending to support their transforms.

## Device input and physical displays

The reported definition is a fixed input contract for each target, selected when the terminal's display path initializes. A terminal using display capabilities to select it uses the primary display at that time as its reference. It may offer a standard input space and perform a separate conversion to the actual panel; it must not label an approximate panel profile as that standard space. Report unavailable information while initialization is incomplete.

The input definition remains fixed for the session, including across window moves, primary-display changes or disconnection, profile updates, filters, and terminal resets. A query does not select or change it. A P3-input session moved to an sRGB display continues interpreting incoming values as P3 and maps them into that display at the output boundary. This provides a stable interpretation, not a guarantee of identical appearance or use of every monitor's maximum gamut.

A terminal must preserve the declared colors through storage and composition and make its output surface's declaration agree with the numbers submitted to it. Converting a shared surface to a wide-gamut basis also requires color-preserving conversion of sRGB text and backgrounds. A surface tag alone must not expand unrelated text colors. The wire protocol prescribes neither GPU texture format nor the internal location of these conversions.

### Policy 3: defined component expansion

Let `D_sRGB` decode an sRGB component and `E_device` encode a linear component in the reported input definition. For policy `3`, apply the following per component:

```text
device_encoded = E_device(D_sRGB(sRGB_encoded))
```

The decoded components become coefficients of the device primaries. No color-preserving primary-conversion matrix is inserted. For Display P3 the transfer curves are identical, so the encoded values remain unchanged. Thus sending the same numbers under P3 policies `2` and `3` produces the same result, although the application's responsibilities differ: policy `2` expects prior conversion, while policy `3` requests expansion of sRGB input.

For `C=1`, the expression is `D_sRGB(u)^(100000/G)`. Arbitrary ICC LUT profiles need not supply a separable, invertible component curve, so accepting ICC under policy `2` does not imply policy `3` support. Advertise `S3=0` if the defined expansion cannot be implemented consistently with the reported color definition. This is a reproducible component mapping, not a perceptual saturation optimizer or a guarantee of colorimetric neutrality on every display.

## Color lifetime, palettes, and resets

Capture the target's policy and fixed input definition when a SIXEL image or colored DRCS download begins, including for asynchronous decoding. Newly defined RGB palette entries use that captured interpretation. HLS entries first expand to normalized RGB using the existing DEC HLS rules, then use the same interpretation. Initial default palette entries retain their sRGB definition.

Changing policy affects future definitions; it does not reinterpret existing images, scrollback, glyphs, or retained palette entries. A receiver with shared or sticky palettes preserves the definition of each entry until it is explicitly redefined. This extension does not itself introduce palette sharing. Redefining a register must preserve the format's paint-time semantics, including colors already painted, and existing modes such as OR-mode retain their specified palette-resolution rules.

A single mutable tag on an entire image cannot describe mixed default sRGB and newly defined device-RGB entries. Keep their provenance, or convert to a common representation at the point where palette values affect pixels. Partial updates likewise interpret old and new pixels in their own definitions before composition, preserving paint masks and transparency. Advertise support only where the target's retained and partial-update paths preserve these semantics.

Policies belong to the terminal session. Normal/alternate-screen switching and SGR reset leave them unchanged. RIS and DECSTR reset both policy values to `0`, while the fixed input definition remains unchanged. DECSC/DECRC do not save or restore these policies. Existing reset rules decide which images, palettes, and glyphs are removed; retained data keeps its original color meaning. Text palette extensions, including any additional color-definition parameter, require their own specification.

## Application negotiation and recovery

For a chosen target, an application performs the following sequence before using device-input RGB:

1. Query `CSI ?T./r`, receive the complete reply, and validate its target, support flags, and color definition. Require `A=1` and `S2=1`, and establish a usable source-to-device transform.
2. Query `DCS $qT./p ST` and save the exact returned policy `M`. A failed state query does not establish a value to restore.
3. Set `CSI T;2./p`, then query the same policy and verify `2` before sending image data.
4. Send image values converted to the reported input definition.
5. Restore `CSI T;M./p` on completion or error, and verify restoration while the reply stream remains usable.

If an available target has no usable device transform, an sRGB fallback saves the policy, sets and verifies `1`, sends sRGB values, and restores the saved policy. Changing only the encoder's output representation is insufficient. An `A=0` reply or a completely silent legacy terminal provides no verified policy; an application may use its historical sRGB assumption, but must identify that as an unverified fallback.

Timeout and malformed or interrupted replies are not evidence that the current policy is sRGB. Applications impose bounded deadlines and own a single reader for terminal replies, coordinating with other query users. A late or unrelated reply must not be mistaken for a successful transaction. If correlation is lost after a setting may have changed, use the saved value for bounded best-effort restoration; do not claim that restoration was verified. Cancel an incomplete outgoing control string or image before sending the restoring CSI.

A multiplexer is responsible for preserving the complete byte stream and response framing. Ordinary transport read splits are handled incrementally. This proposal adds no fragment reassembly to compensate for inserted data and provides no isolation between simultaneous writers or applications sharing terminal policy.

### Limited `img2sixel` integration

Automatic selection is a CLI operation requested by `-Uauto`, restricted initially to ordinary SIXEL output actually directed to a TTY. File/pipe output and explicit output-space choices do not perform these color queries. The default output choice remains `gamma`. For a P3-capable target, the CLI resolves the response to `display-p3`, manages the setting transaction, and passes that concrete output choice to the existing selected encoder path. An available sRGB target uses verified policy `1` and `gamma` output. The CLI rejects a final automatic request combined with DRCS (`-@`) before color queries or image output because this implementation handles `T=1` only; it does not negotiate the independent `T=2` DRCS policy.

The initial CLI route is stdout only, with no `-o` option or with every `-o` argument exactly `-`. A file destination, a pipe, or any other `-o` argument excludes automatic device-color queries even if stderr is a TTY. A separate read/write descriptor must identify that same stdout terminal; an unrelated controlling terminal is not a substitute. This keeps saved or forwarded SIXEL in the default gamma sRGB representation instead of silently binding its interpretation to the terminal used while creating it. The CLI does not infer the eventual consumer beyond a pipe. Explicit output-space choices remain available when the user knows the consumer's interpretation.

The CLI owns TTY access, reply parsing, deadlines, fallback decisions, and policy restoration. The library receives a resolved output intent and performs color conversion and encoding; it does not discover a terminal from stdout or introduce terminal reads as a consequence of choosing P3. Initial automatic support accepts the sRGB and Display P3 named definitions. Parsing a simple or ICC response does not imply that the corresponding output transform is implemented. The current implementation and its supported loader/quantizer combinations remain documented in [Output color space](../functionality/output-colorspace.md).

## Color-filter status query

The following DECDSR numbers are proposed from the `8800` extension range. Query and response numbers are distinct:

```text
request: CSI ?8821n
reply:   CSI ?8822;K1;A1;K2;A2;K3;A3n
```

The request has exactly the single private parameter `8821`. The reply uses `8822`, which must not be treated as a new request. Extra request parameters are invalid. Each `K` and `A` is an explicit Boolean, not a bit mask.

| Pair | Observed class |
| --- | --- |
| `K1`, `A1` | Scheduled or time-dependent color-temperature adjustment, such as Night Shift. |
| `K2`, `A2` | Ambient-light-dependent white adjustment, such as True Tone. |
| `K3`, `A3` | Accessibility color transformations, including color filters and inversion. |

| `K;A` | Observation |
| --- | --- |
| `0;0` | Unknown: application has not been observed, but absence has not been established. |
| `1;0` | Complete observation of the class and current display scope; no application. |
| `0;1` | Application observed in part of the scope, with incomplete overall observation. |
| `1;1` | Complete observation; application active in some or all of the scope. |

The scope is the displays actually presenting the terminal and their OS output processing. Observing an unused primary display is insufficient. If any active display applies a filter, report `A=1`; report `K=1` only when the full relevant scope is known. An enabled preference or a scheduled future change does not prove current application. Ordinary ICC conversion/calibration and HDR tone mapping are outside these three classes.

A supporting terminal with no reliable observation returns `CSI ?8822;0;0;0;0;0;0n`. Unknown is neither a verified disabled state nor protocol nonresponse. The result is a snapshot, not a subscription, filter-control interface, or promise about display hardware picture modes. Applications serialize these queries; no request identifier or unsolicited update is added. The neighboring `8840..8842` allocation is not used by this proposal.

## Verification and implementation status

This document defines proposed behavior, not a cross-terminal support matrix. The current repository's [output-color contract tests](../functionality/output-colorspace.md#test-coverage) protect selected encoder values and palette behavior. The linked CLI tests exercise selected automatic negotiation with scripted peers, including base-9999 simple-definition syntax and reserved-parameter rejection. They do not establish cross-terminal adoption, simple/ICC output transforms, or physical display accuracy.

A conforming implementation needs exact byte tests for settings and replies, target isolation, defaults and reserved fields, incremental framing, cancellation, invalid definitions, and restoration. Color tests must independently check sRGB preservation, P3 primary retention, policy `2` versus `3`, mixed palette provenance, retained/partial updates, and agreement between output surface values and their color declarations. Test simple definitions and ICC transforms against independent numeric expectations; a perceptual image score cannot substitute for those checks.

Screenshots are supplementary observations whose profiles and viewers affect appearance. Numeric surface checks and declared color spaces do not measure a physical panel. Filter observations must retain their known/unknown distinction. CLI transport tests also verify actual TTY routing, silent and unsupported receivers, timeouts, failure cleanup, and exclusion of file/pipe output without depending on a named terminal implementation.
