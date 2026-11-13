---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# PF1550 Driver

<p>A C++20 driver for the PF1550 power management IC. It starts the chip, checks the charger against limits
you declare, writes a typed configuration with read-back, and controls the charger, the bucks, the LDOs and the
power states, on any MCU.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-pf1550){ .md-button }
</div>

The driver has no platform code. You give it a small class that brings the bus up and moves bytes; it does the
rest: the limit and acknowledgement checks, the register framing, the write order the datasheet needs, the read-backs,
the bounded waits and the error mapping.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">Power management IC</span>
<span class="kb-chip__name">PF1550</span>
<span class="kb-chip__vendor">NXP Semiconductors</span>
</div>

Charger
:   Linear single-cell charger with power path, 3.5 V to 4.44 V, 100 mA to 1 A, JEITA, watchdog, LED driver

Regulators
:   SW1 and SW2 buck with DVS, SW3 buck, LDO1 to LDO3, VREFDDR, VSNVS, coin cell charger

Interface
:   I2C up to 400 kHz; 7-bit address `0x08` to `0x0F` set by OTP; single-register transfers

States
:   Run, Standby, Sleep, SHIP, CORE_OFF; power-up and power-down sequencer

Interrupts
:   INTB, 31 sources in 8 groups, each with a status, mask and sense register

Supply
:   Charger input 4.1 V to 6.0 V; I/O 1.7 V to 3.3 V
</div>

## What you get

- **One object per chip.** `PF1550<YourPort>` takes a reference to your port and a `Config`, and does no I/O until
  `EnsureInitialized()`. See [API guide](api-guide.md).
- **Safety by construction.** No public raw write; mandatory `Limits`; acknowledgements for every downgrade; every
  write read back. See [Contract](contract.md) and [Configuration](configuration.md).
- **Errors you can act on.** Every call returns `Result<T>` with an `Error` such as `Bus`, `Unsafe`, `OutOfLimits` or
  `Refused`. See [API guide](api-guide.md#handle-errors).
- **The whole chip.** Charger, bucks, LDOs, VSNVS, coin cell, USBPHY, LED, power states, sequencer and interrupts; the
  [coverage](coverage.md) page lists what is modelled and what is not.
- **Zero-overhead portability.** The port is a CRTP class checked by a concept: the bus hooks and a delay, plus
  optional STANDBY, INTB, failure-report and logging hooks found at compile time. See [Porting](porting.md).
- **No heap, no exceptions, no RTTI.** The host tests build with `-fno-exceptions -fno-rtti`.
- **Tested without hardware.** Host tests run the driver against an independent register model of the chip. See
  [Testing and CI](testing.md). Nothing is verified on hardware yet.

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: PF1550 Power management integrated circuit (PMIC) for low power application processors (NXP Semiconductors PF1550) | 7, 2021-09-29 | [vendor page](https://www.nxp.com/docs/en/data-sheet/PF1550.pdf) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the chip, write the port, start the driver and read the charger.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, bus settings, the package and the power.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, what it needs, threading, errors and the open questions.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Start, configure, charge, control rails and handle errors, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the port for your MCU.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
