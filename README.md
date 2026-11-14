# PF1550 Driver

<!-- generated: chip-summary -->
The **PF1550** is a power management IC for battery-powered boards built around a low-power application processor: it
charges a single Li-ion cell and supplies the processor, the memory and the peripherals from three buck converters,
three linear regulators and an RTC supply (1 General description, p. 1).
<!-- /generated -->

C++20 driver for the NXP PF1550 power management IC (single-cell Li-ion linear charger, three buck regulators, three
LDOs) on I2C

[![CI](https://github.com/kokeb-labs-drivers/driver-pf1550/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kokeb-labs-drivers/driver-pf1550/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-informational)](docs/installation.md#requirements)

**Documentation: [`docs/`](docs/index.md)**, with the API reference built by Doxygen ([how](docs/api-reference.md)).
The site at <https://kokeb-labs-drivers.github.io/driver-pf1550/> opens when the repository is public.

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-hero.dark.svg">
  <img src="docs/diagrams/chip-hero.svg"
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

## What it does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-capabilities.dark.svg">
  <img src="docs/diagrams/chip-capabilities.svg"
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

## Where it is used

<!-- generated: chip-uses -->
- Smart mobile and wearable devices (1.2, p. 2)
- Low-power IoT devices (1.2, p. 2)
- Wireless game controllers (1.2, p. 2)
- Embedded monitoring systems (1.2, p. 2)
- Home automation (1.2, p. 2)
- Point-of-sale terminals (1.2, p. 2)
- E-readers (1.2, p. 2)

Uses named by the datasheet (1 General description, p. 1).
<!-- /generated -->

## Inside the chip

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-blocks.dark.svg">
  <img src="docs/diagrams/chip-blocks.svg"
    alt="Block diagram of the PF1550: its function blocks, how they connect and which package pins reach each
    block.">
</picture>
<!-- /figure -->

## Pins and wiring

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-pinout.dark.svg">
  <img src="docs/diagrams/chip-pinout.svg"
    alt="Pinout of the PF1550 in HVQFN-40, top view: 40 pins and an exposed pad, each with its name, its
    electrical type as a colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-wiring-typical.dark.svg">
  <img src="docs/diagrams/chip-wiring-typical.svg"
    alt="Wiring of the PF1550 for the Control connections to a host processor: Host I2C SDA to SDA (Write(),
    WriteRead()); Host I2C SCL to SCL (Write(), WriteRead()); Host GPIO (STANDBY) to STANDBY
    (GpioSet(CtrlPin::Standby)); Host GPIO (INTB) to INTB (GpioGet(CtrlPin::Intb)). Supplies, grounds
    and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

| MCU | PF1550 | Notes |
| --- | --- | --- |
| I2C SCL | SCL | Pull-up to VDDIO; up to 400 kHz |
| I2C SDA | SDA | Pull-up to VDDIO |
| GPIO output | STANDBY | Optional: lets `RequestStandby()` drive the pin |
| GPIO input | INTB | Optional, open drain: lets `ServiceInterrupts()` skip the bus while INTB is high |
| 1.7 V to 3.3 V | VDDIO | The I/O supply |
| GND | GND, VDDOTP, exposed pad | |

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/chip-outline.dark.svg">
  <img src="docs/diagrams/chip-outline.svg"
    alt="Outline of the PF1550 in HVQFN-40 drawn to scale from above: body 5.0 x 5.0 mm, 40 pins and an
    exposed pad.">
</picture>
<!-- /figure -->

<!-- generated: chip-card -->
| | | Datasheet |
| --- | --- | --- |
| **Package** | HVQFN-40, exposed pad | 4.1, p. 7 |
| **Pins** | 40 | 4.1, p. 7 |
| **Body size** | 5.0 x 5.0 mm | 14, p. 139 |
| **Supply VDDIO** | 1.7 V to 3.3 V | 4.2, p. 8 |

From [`docs/hardware.yml`](docs/hardware.yml); pins, circuits and the block diagram: [Hardware](docs/hardware.md).
<!-- /generated -->

## Quick start

**1. Wire it** as in [Pins and wiring](#pins-and-wiring).

**2. Write the port**: a class with `EnsureInitialized()` (bring the bus up), `Write()` and `WriteRead()` (one I2C
transaction each) and `DelayUs()`, about 25 lines on most SDKs. [Porting](docs/porting.md) has the contract and a
complete example.

**3. Describe your hardware and start the chip.** `Limits` is what your cell, supply and loads tolerate. It has no
defaults: a configuration without it is rejected before anything is sent.

<!-- generated: quickstart-code -->
```cpp
#include "pf1550.hpp"

int start_pmic(MyPort& port) {
  pf1550::Config config{};
  config.limits.cell_max_mv = 4200;          // from your cell's data sheet
  config.limits.cell_max_charge_ma = 500;    // from your cell's data sheet
  config.limits.input_max_ma = 1000;         // what your supply delivers
  pf1550::PF1550<MyPort> pmic(port, config); // no I/O yet

  if (auto r = pmic.EnsureInitialized(); !r) { // 1. bus up, DEVICE_ID, charger as found
    return static_cast<int>(r.error); //    NoDevice, DeviceIdMismatch, Unsafe, OutOfLimits ...
  }
  const auto status = pmic.ReadChargerStatus(); // 2. read only
  if (!status) {
    return static_cast<int>(status.error);
  }
  return static_cast<int>(status.value.fault); // 3. 0 is Fault::None
}
```
<!-- /generated -->

`EnsureInitialized()` is the only call that starts the driver. Every other call returns `Error::NotInitialized`
until it has succeeded, and no call ever starts the chip on its own. At start-up the driver also reads the charger
as found: some pre-programmed options turn it on with the charge timer off (Table 85 of the datasheet), and the
driver reports that against your limits before it writes anything.

## Driver coverage

<!-- generated: chip-coverage -->
| Capability | What the chip does | Driver API | Registers | Status | Datasheet |
| --- | --- | --- | --- | --- | --- |
| [Battery charger](docs/api-guide.md#charge) | Charges one Li-ion cell: charge voltage and current, termination, timers, watchdog and the status LED (charging starts only through SetChargerMode, within the Limits you declare) | `SetChargerMode`, `RestartCharger`, `ServiceChargerWatchdog`, `ReadChargerStatus`, `CheckChargerAgainstLimits` | `CHG_OPER`, `BATT_REG`, `CHG_CURR_CNFG`, `CHG_EOC_CNFG`, `CHG_TMR`, `LED_CNFG` | Supported | 8, p. 42 |
| [Power path](docs/configuration.md#charger-chargerconfig) | Runs the system from the input while the cell charges: input current limit, VSYS minimum, battery FET | `Configure`, `VbusOptions`, `BatfetConfig` | `VBUS_INLIM_CNFG`, `VBUS_LIN_DPM`, `VBUS2SYS_CNFG`, `BATFET_CNFG` | Supported | 8.7, p. 55 |
| [Battery thermistor and JEITA](docs/configuration.md#charger-chargerconfig) | Reads the cell's thermistor and lowers or stops the charge when the cell is too cold or too hot | `Configure`, `JeitaConfig` | `THM_REG_CNFG`, `THM_ADJ_SETTING`, `THM_COLD`, `THM_HOT` | Supported | 8.7.3, p. 56 |
| [Buck regulators](docs/api-guide.md#rails) | Sets SW1 to SW3 per state (run, standby, sleep), with dynamic voltage scaling on SW1 and SW2 | `SetRailVoltage`, `SetRailModes`, `ReadRail`, `BuckConfig` | `SW1_VOLT`, `SW1_CTRL`, `SW2_VOLT`, `SW2_CTRL`, `SW3_VOLT`, `SW3_CTRL` | Supported | 6.1, p. 25 |
| [LDOs, VREFDDR and VSNVS](docs/api-guide.md#rails) | Sets LDO1 to LDO3, the DDR reference, the RTC supply, the coin cell charger and the USB PHY regulator (the LDO1 and LDO3 load switch mode needs a confirmation token) | `SetRailVoltage`, `SetRailModes`, `EnableLoadSwitch`, `LdoConfig`, `CoinCell` | `LDO1_VOLT`, `LDO1_CTRL`, `VREFDDR_CTRL`, `VSNVS_CTRL`, `COINCELL_CONTROL`, `USB_PHY_LDO_CNFG` | Supported | 7, p. 34 |
| [Interrupts and status](docs/api-guide.md#interrupts) | Pulls INTB low on a fault or event; the driver reads, latches and clears each interrupt group | `ServiceInterrupts`, `SetIrqMask`, `ReadCategories`, `GetLatchedEvents`, `ReadSnapshot` | `INT_CATEGORY`, `CHG_INT`, `SW_INT_STAT0`, `LDO_INT_STAT0`, `MISC_INT_STAT0` | Supported | 9.4, p. 70 |
| [Power states and sequencer](docs/api-guide.md#power-states) | Run, standby, sleep, core off and ship states, with the order rails start and stop in (SHIP and CORE_OFF need a confirmation token, the power-down order an acknowledgement) | `RequestStandby`, `EnterShip`, `EnterCoreOff`, `ReadPowerState`, `SetPowerDownSequence` | `PWRCTRL0`, `PWRCTRL3`, `STATE_INFO`, `SW1_PWRDN_SEQ`, `LDO1_PWRDN_SEQ` | Guarded | 10, p. 72 |
| [OTP start-up configuration](docs/datasheet.md#what-is-not-covered) | Fuses hold the start-up sequence, voltages and charger defaults; the driver reads the option, never fuses | `ReadVariant` | `OTP_FLAVOR` | Not covered | 11.2, p. 79 |

**Supported**: the driver covers it and a host test checks it; **Guarded**: covered, behind an explicit acknowledgement
because a wrong value can do harm; **Not covered**: the chip has it and the driver leaves it alone.

From [`docs/hardware.yml`](docs/hardware.yml); `kokeb-docs hardware --check` confirms that every API name is in
`inc/pf1550.hpp`, `inc/pf1550_config.hpp`, `inc/pf1550_types.hpp` and every register in `inc/pf1550_registers.hpp`.
<!-- /generated -->

A wrong byte can overcharge a cell or overvolt a processor, so this driver has no public raw register write: every
setting is typed, checked against limits you declare, written and read back. It runs on any MCU: you give it a small
class that moves bytes on your I2C bus, and it does the rest.

| | |
| --- | --- |
| **Bus** | I2C up to 400 kHz, 7-bit address `0x08` to `0x0F` set by OTP, single-register transfers |
| **Also drives** | STANDBY (optional `GpioSet()`) and reads INTB (optional `GpioGet()`) |
| **Driver** | Header-only C++20; no heap, exceptions, RTTI or virtual calls; every call returns `Result<T>` |
| **Safety** | Mandatory `Limits`, explicit acknowledgements, read-back of every write, guarded SHIP and CORE_OFF |
| **Runs on** | Any MCU with I2C and a C++20 compiler; the host example runs on any PC ([Platforms](docs/platforms.md)) |
| **Status** | 0.1.0, not yet verified on hardware: host-tested against a register model of the chip |

- Starts the driver safely: validates the configuration, checks DEVICE_ID, checks the charger as found and
  reports or switches off an unsafe charge (your choice, `AsFoundPolicy`).
- Writes the charger (voltage, current, input limit, timers, thermistor, JEITA, watchdog, battery FET) in the order
  the datasheet needs, with the mode last, and enables charging only through `SetChargerMode()`.
- Controls the three bucks (DVS voltage steps with a bounded DVS-done wait), the three LDOs (low-power mode, one-way
  load switch), VREFDDR, VSNVS, the coin cell charger, the USBPHY LDO and the charger LED.
- Reads the state machine, the charger status with a classified fault, every interrupt group, and a diagnostic
  snapshot; handles INTB with clear-after-copy semantics.
- Guards the dangerous: SHIP and CORE_OFF need a confirmation token and their preconditions; the power-down order
  needs an acknowledgement; hard-reset paths and the undervoltage threshold need one each.
- Models the power-up and power-down sequencer of the datasheet as pure `constexpr` functions.
- Returns `Result<T>` from every call: no exceptions, no heap, no RTTI, no virtual calls.

## Documentation

| Page | Read it when you want to |
| --- | --- |
| [Quickstart](docs/quickstart.md) | Go from a bare chip to a configured charger and rail on your MCU |
| [Hardware](docs/hardware.md) | Wire it, check the bus settings, the pins and the power |
| [Porting](docs/porting.md) | Write the port class for your MCU's SDK |
| [API guide](docs/api-guide.md) | Use each call: start, configure, charge, rails, interrupts, power states |
| [Configuration](docs/configuration.md) | Choose every setting, the limits and the acknowledgements |
| [Troubleshooting](docs/troubleshooting.md) | Find the cause of an error or an unexpected state |
| [Contract](docs/contract.md) | Know exactly what the driver promises, needs and returns, and the open questions |
| [Datasheet map](docs/datasheet.md) | Find the datasheet section behind each part of the driver, and what is not covered |
| [Coverage](docs/coverage.md) | See which parts of the chip are modelled, tested or deliberately left out |
| [API reference](docs/api-reference.md) | Look up a class or member (Doxygen; built by CI and locally) |

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: PF1550 Power management integrated circuit (PMIC) for low power application processors (NXP Semiconductors PF1550) | 7, 2021-09-29 | [vendor page](https://www.nxp.com/docs/en/data-sheet/PF1550.pdf) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](docs/datasheets.yml). Where each
part of the driver is in the datasheet: [Datasheet map](docs/datasheet.md).
<!-- /generated -->

Every docs page lists the datasheet sections it relies on, and code comments cite the section they implement.

## Try it on your PC

No hardware needed: [`examples/host`](examples/host/README.md) runs the driver against a simulated chip.
You need git, CMake 3.16+ and a C++20 compiler (GCC 10+, Clang 13+ or MSVC 2022).

1. Get the source:

   ```bash
   git clone https://github.com/kokeb-labs-drivers/driver-pf1550.git && cd driver-pf1550
   ```

2. Build and run the example:

   ```bash
   cmake -S examples/host -B build-example
   cmake --build build-example
   ./build-example/pf1550_host_example
   ```

3. You should see the driver start the simulated chip, configure the charger and LDO1, and read them back:

   ```text
   PF1550 driver 0.1.0 on the host
   pf1550: initialised
   charger state 1, fault 0, VBUS valid 1
   LDO1 1800000 uV, run 1
   0 interrupt passes
   ```

To run the whole test suite instead: `cmake -S . -B build -DPF1550_BUILD_TESTS=ON && cmake --build build && ctest
--test-dir build`.

## Add it to your project

Pick the row that matches your build; each is a few steps, and [Installation](docs/installation.md) has the
details.

| Your build | Steps below |
| --- | --- |
| Plain CMake: STM32 (CubeMX), RP2040 (Pico SDK), nRF Connect, a host app | [A](#a-cmake-any-mcu) |
| ESP-IDF | [B](#b-esp-idf) |
| An SDK or OS image that installs libraries | [C](#c-installed-package) |

### A. CMake, any MCU

1. Add the driver as a submodule, pinned to the version you chose:

   ```bash
   git submodule add https://github.com/kokeb-labs-drivers/driver-pf1550.git external/driver-pf1550
   ```

2. In your `CMakeLists.txt`, after `project()`:

   ```cmake
   add_subdirectory(external/driver-pf1550)
   target_link_libraries(my_app PRIVATE pf1550::pf1550)   # brings the headers and C++20
   ```

3. In your code, `#include "pf1550.hpp"`, write the port ([Porting](docs/porting.md)) and build. A first build
   that compiles the quickstart above is the check that the driver is wired in.

### B. ESP-IDF

1. Add the driver under your project's `components/`; ESP-IDF finds it there and names it after the folder:

   ```bash
   git submodule add https://github.com/kokeb-labs-drivers/driver-pf1550.git components/pf1550
   ```

2. In `main/CMakeLists.txt`, list it: `idf_component_register(SRCS "main.cpp" REQUIRES pf1550)`.
3. `idf.py menuconfig` now has a **PF1550 power management IC** menu (cell voltage and current, supply current,
   the as-found policy), and `idf.py build` builds the driver with your app. [`examples/esp32`](examples/esp32) is a
   complete project with a port for the board.

### C. Installed package

1. Install it once into a prefix:

   ```bash
   cmake -S external/driver-pf1550 -B build-pf1550 && cmake --install build-pf1550 --prefix /opt/sdk
   ```

2. In your project, with `CMAKE_PREFIX_PATH=/opt/sdk`:

   ```cmake
   find_package(pf1550 0.1 REQUIRED)
   target_link_libraries(my_app PRIVATE pf1550::pf1550)
   ```

The target is `pf1550::pf1550` every way, so switching later changes nothing else.

## Tests and quality

```bash
cmake -S . -B build -DPF1550_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

The host tests run the driver against an independent register model of the chip (reset values, access types,
the sequencer, DVS timing, interrupts), fail every bus transfer in turn, and produce each error the driver can
return. A fuzzer fails, corrupts and replaces what the chip sends back and checks the
driver's promises after every call. CI also builds for Cortex-M and gates coverage, complexity, sanitizers,
fuzzing, clang-tidy, cppcheck and the handbook rules (`.kokeb/handbook.yml`). See [Testing and CI](docs/testing.md),
which lists every gate and its limit.

## License

MIT. See [LICENSE](LICENSE).
