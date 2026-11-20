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
