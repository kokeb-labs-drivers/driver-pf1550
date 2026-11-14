# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the PF1550

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the PF1550: its function blocks, how they connect and which package pins reach each
    block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## What the PF1550 does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the PF1550: Battery charger (supported): Charges one Li-ion cell: charge voltage
    and current, termination, timers, watchdog and the status LED; Power path (supported): Runs the
    system from the input while the cell charges: input current limit, VSYS minimum, battery FET;
    Battery thermistor and JEITA (supported): Reads the cell's thermistor and lowers or stops the charge
    when the cell is too cold or too hot; Buck regulators (supported): Sets SW1 to SW3 per state (run,
    standby, sleep), with dynamic voltage scaling on SW1 and SW2; LDOs, VREFDDR and VSNVS (supported):
    Sets LDO1 to LDO3, the DDR reference, the RTC supply, the coin cell charger and the USB PHY
    regulator; Interrupts and status (supported): Pulls INTB low on a fault or event; the driver reads,
    latches and clears each interrupt group; Power states and sequencer (guarded): Run, standby, sleep,
    core off and ship states, with the order rails start and stop in; OTP start-up configuration (not
    covered): Fuses hold the start-up sequence, voltages and charger defaults; the driver reads the
    option, never fuses.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## PF1550: Control connections to a host processor

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the PF1550 control connections to a host processor: the chip with U2 Host, R1 resistor
    2.2 kΩ, R2 resistor 2.2 kΩ, R3 resistor 10 kΩ; the MCU wires carry the port hooks
    GpioGet(CtrlPin::Intb), GpioSet(CtrlPin::Standby), Write(), WriteRead().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## PF1550

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the PF1550, a power management IC for battery-powered boards built around a low-power
    application processor: it charges a single Li-ion cell and supplies the processor, the memory and
    the peripherals from three buck converters, three linear regulators and an RTC supply. A MCU talks
    to it over I2C; it drives Supply rails, then Processor and memory; it reads back Charger input, Li-
    ion cell, Push buttons. Headline figures: 4.1 to 6.0 V charger input on VBUSIN, USB or adapter; 3 x
    1.0 A buck converters, two with voltage scaling; 3 LDOs 300 mA, 400 mA and 300 mA; up to 1 A charge
    current from 100 mA, charge voltage 3.5 to 4.44 V; 400 kHz I2C, fast mode, 7-bit address; 5 x 5 mm
    40-pin QFN with exposed pad.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## PF1550 package, HVQFN-40

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the PF1550 in HVQFN-40 drawn to scale from above: body 5.0 x 5.0 mm, 40 pins and an
    exposed pad.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## PF1550 pinout, HVQFN-40

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the PF1550 in HVQFN-40, top view: 40 pins and an exposed pad, each with its name, its
    electrical type as a colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## Wiring: Control connections to a host processor

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the PF1550 for the Control connections to a host processor: Host I2C SDA to SDA (Write(),
    WriteRead()); Host I2C SCL to SCL (Write(), WriteRead()); Host GPIO (STANDBY) to STANDBY
    (GpioSet(CtrlPin::Standby)); Host GPIO (INTB) to INTB (GpioGet(CtrlPin::Intb)). Supplies, grounds
    and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the PF1550 core, which sends register reads and writes through
    the I2C port contract to the port class you write, which drives the chip; the core and the contract
    are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/pf1550.hpp`](../../inc/pf1550.hpp)
- `I2cInterface` in [`inc/pf1550_i2c_interface.hpp`](../../inc/pf1550_i2c_interface.hpp)

## One register read on the I2C bus

<!-- figure: i2c-register-read -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="i2c-register-read.dark.svg">
  <img src="i2c-register-read.svg"
    alt="A register read on the PF1550 I2C bus: the host writes the register address 0x00, repeats the start
    with the read address, reads one byte and ends with NACK and STOP.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/i2c-register-read.seq.yml`](../model/i2c-register-read.seq.yml)

Drawn from:

- [`docs/model/i2c-register-read.seq.yml`](../model/i2c-register-read.seq.yml)

## One call through the layers

<!-- figure: one-call-through-the-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="one-call-through-the-layers.dark.svg">
  <img src="one-call-through-the-layers.svg"
    alt="One call through the layers: your code calls SetRailVoltage(Ldo1, 2.8 V), the core exchanges frames
    with the chip through your port and returns the result to your code.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)

Drawn from:

- [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)
