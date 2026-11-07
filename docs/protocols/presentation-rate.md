# Terminal Presentation Rate Proposal

Status: experimental protocol proposal, revised 2026-11-07. This document proposes control sequences through which an application that sends SIXEL frames, such as a video or animation player, learns how often a terminal can show them, asks the terminal to show them less often, and hears when that rate changes. It does not assert adoption by existing terminals or registration of the proposed selectors. Support must be discovered from the receiving session, not inferred from a terminal name, operating system, or display model.

A player that sends frames faster than the terminal presents them spends encoding time and transport bandwidth on frames nobody sees. Ordinary SIXEL carries no timing information, and nothing in the stream tells the sender the receiver's pace. The exchange below supplies that information outside the image stream. Frame timing on the sending side, including how `img2sixel` derives delays from animated inputs, is described in [Animation](../encoder/animation.md). Color interpretation is a separate proposal, the [terminal graphics color management proposal](graphics-color-management.md).

## Scope and notation

The examples use the 7-bit forms, framing, parameter byte classes, and numeric field rules defined in [Scope and notation of the color management proposal](graphics-color-management.md#scope-and-notation). In particular, numeric fields are unsigned ASCII decimal integers in `0..9998`, and receivers wait for a complete sequence before acting on it. The selector `./q` contains two intermediate bytes, `.` and `/`, followed by final byte `q`.

The exchange concerns one presentation context of the receiver: the screen area that shows the session's output, such as one terminal tab. A receiver with several contexts keeps the application limit, the notification mode, and the reported rates separately for each.

Every rate is a frequency in millihertz, and zero means unknown or no request. Because a single parameter cannot carry a value above `9998`, each rate travels as two fields: whole hertz, then a millihertz fraction in `0..999`. For example, 59.94 Hz travels as `59;940`, and 120 Hz as `120;0`.

## Rates

Four rates take part. The table defines each one and where it comes from.

| Rate | Name | Source |
| --- | --- | --- |
| D | Display rate | The maximum refresh rate of the display that shows the context. The receiver observes it and may follow display changes. It is zero when the receiver cannot see a display, for example when its output is not a local screen. |
| U | User limit | A receiver-side user preference that lowers the pace, or zero when the user sets none. |
| A | Application limit | The last limit the application set with the sequence below, or zero. |
| E | Effective rate | The smallest nonzero member of D, U, and A, using 60 Hz in place of an unknown D. |

U and A can only lower the pace. A request above D does not raise E above the display; the report marks that case. The examples show how E follows from the other three:

| D | U | A | E |
| --- | --- | --- | --- |
| 120 Hz | 0 | 0 | 120 Hz |
| 120 Hz | 0 | 30 Hz | 30 Hz |
| 60 Hz | 0 | 144 Hz | 60 Hz, marked as capped by the display |
| 0 | 0 | 0 | 60 Hz |

E is a target, not a guarantee. How closely the shown frame rate follows E is up to the receiver, and it may be lower, for example when decoding cannot keep up, or somewhat higher, for example when text updates and image updates are paced separately. An application should treat E as the rate at which new frames are worth sending, not as a measurement of what was shown.

## Feature probe

```text
request: CSI ?8890$k
reply:   CSI ?8890;Ps;Pv;0;0;Pc$k
```

`Ps=1` means the receiver supports this proposal, and `Pv=1` is its version. The two zero fields are reserved. `Pc` is a bit set describing D:

| Bit | Meaning |
| --- | --- |
| `1` | D was observed from a display. |
| `2` | D follows display changes, such as moving the window to another screen. |

`Pc` is `3` once the receiver has observed a display that it tracks, and `0` while D is unknown. Any other `Ps`, a different reply, or no reply within the application's deadline means the proposal is not available, and the application keeps its own pacing.

## Query the presentation rate

```text
request: CSI ?8890n
report:  CSI ?8891;Ei;Ef;Di;Df;Ui;Uf;Ai;Af;Pf n
```

The request has exactly the single private parameter `8890`. The report uses `8891`, which must not be treated as a new request. Each `i;f` pair is one rate in whole hertz and millihertz fraction, in the order E, D, U, A. `Pf` is a bit set:

| Bit | Meaning |
| --- | --- |
| `1` | The display has a variable refresh rate, and D is its maximum. |
| `2` | U or A asked for more than D, so D capped the request. |

For example, `CSI ?8891;120;0;120;0;0;0;0;0;1n` reports a variable-rate 120 Hz display with no limits, and `CSI ?8891;30;0;120;0;0;0;30;0;1n` reports the same display after the application set a 30 Hz limit.

## Set the application limit

```text
CSI Ai;Af./q
```

The sequence sets A for the context and has no reply. An omitted, empty, or zero rate clears A. A rate below 1 Hz or above 1000 Hz, a fraction above `999`, three or more parameters, or malformed syntax makes the whole sequence a no-op and leaves A unchanged. Senders use the canonical forms `CSI 30./q` for a whole rate, `CSI 59;940./q` for a fractional one, and `CSI ./q` to clear.

The limit is advisory. A receiver lowers E to it, but whether and how strictly the shown frames follow E is implementation-dependent, as stated under [Rates](#rates). Setting a limit therefore does not replace pacing on the sending side; it lets the receiver avoid presenting faster than the application intends and lets other parts of the receiver use the time saved.

## Change notification

```text
enable:  CSI ?8890h
disable: CSI ?8890l
status:  CSI ?8890$p  ->  CSI ?8890;Pm$y
```

Private mode `8890` turns on unsolicited reports. Enabling it sends one report at once, in the query's report format, as a baseline. After that the receiver sends a report only when a reported field changes, for example when the window moves to a display with another refresh rate, the user changes U, or the application changes A. An unchanged recomputation stays silent, and repeating the same limit sends nothing. Disabling the mode stops reports; enabling it again sends a fresh baseline. DECRQM reports `Pm=1` when the mode is set and `Pm=2` when it is reset. The mode starts reset.

A receiver may deliver a display change to a context only when that context becomes visible again. An inactive tab, for example, shows no frames, so its report may arrive on its first update after it is selected.

## Resets

RIS (`ESC c`) and DECSTR (`CSI !p`) clear A and reset mode `8890`. D and U describe the receiver and its user, not the application's stream, so a reset leaves them unchanged. After a reset, a query reports the rates without the application limit.

## Application negotiation

The figure shows the exchange of a player that adapts to the receiver. It probes once, queries the rates, optionally sets a limit, and listens for changes while it plays.

```mermaid
sequenceDiagram
    participant App as Application
    participant Term as Terminal
    App->>Term: CSI ?8890$k
    Term-->>App: CSI ?8890;1;1;0;0;3$k
    App->>Term: CSI ?8890n
    Term-->>App: CSI ?8891;120;0;120;0;0;0;0;0;1n
    App->>Term: CSI 30./q
    App->>Term: CSI ?8890h
    Term-->>App: CSI ?8891;30;0;120;0;0;0;30;0;1n
    Note over Term: window moves to a 60 Hz display
    Term-->>App: CSI ?8891;30;0;60;0;0;0;30;0;0n
```

An application waits for each reply with a deadline and treats a missing reply as no support, without blocking playback. Because reports can arrive while the application also expects other replies, it parses `CSI ?8891` reports wherever they appear in its input. Before it exits, an application that set a limit or enabled notification clears them with `CSI ./q` and `CSI ?8890l`, so that a later program in the same session starts from the receiver's own rate.

## Verification and implementation status

This document defines proposed behavior, not a cross-terminal support matrix. libsixel and `img2sixel` neither send these sequences nor adapt their output to the reported rate, and the repository has no tests for this exchange.

A conforming receiver needs exact byte tests for the probe, the query report, and the DECRQM reply. It also needs tests for every accepted and rejected form of the limit, for the rule that E is the smallest nonzero rate with the 60 Hz fallback, and for the capped and variable-rate flags. Notification tests check the baseline on enable, silence while nothing changes, a report for each changed field, a fresh baseline after re-enabling, and the effect of RIS and DECSTR. Shown frame rates on a physical display are observations of one receiver and its display, not evidence for the protocol, because the proposal leaves strictness to the implementation.
