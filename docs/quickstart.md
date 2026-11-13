# Quickstart

Run the driver on your PC first, with no hardware; then start the PMIC from your MCU: wire the chip, write a small
port class, declare what your cell and supply tolerate, and call three driver functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NXP Semiconductors PF1550 7:

    - Pins and package: 4.2 Pin definitions, p. 8

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Try it on your PC

[`examples/host/`](../examples/host/README.md) runs the real driver against a simulated PF1550, through a host
port. From the repository root, with CMake and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/pf1550_host_example
```

```text
PF1550 driver 0.1.0 on the host
pf1550: initialised
charger state 1, fault 0, VBUS valid 1
LDO1 1800000 uV, run 1
0 interrupt passes
```

The calls in [`main.cpp`](../examples/host/main.cpp) are the ones below; only the port differs on an MCU. CI builds
and runs this example on every pull request.

## 1. Wire it

| MCU | PF1550 | Notes |
| --- | --- | --- |
| I2C SCL | SCL | Pull-up to VDDIO; up to 400 kHz |
| I2C SDA | SDA | Pull-up to VDDIO |
| GPIO output | STANDBY | Optional, for `RequestStandby()` |
| GPIO input | INTB | Optional, open drain, active low |
| 1.7 V to 3.3 V | VDDIO | The I/O supply |

The chip answers at `0x08` unless its OTP set another address (`0x08` to `0x0F`); pass the address as the third
constructor argument. The [Hardware](hardware.md) page has the full pin list, and the datasheet's example
schematic the power connections (battery, supply, regulator outputs and capacitors).

## 2. Add the driver

Add the repository as a submodule and link the `pf1550::pf1550` CMake target. [Installation](installation.md)
covers CMake on any MCU, the installed package and ESP-IDF.

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-pf1550.git external/driver-pf1550
```

## 3. Write the port

The port is the only platform code: it brings the bus up and moves bytes. Replace the `platform_*` calls with your
SDK's.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

[Porting](porting.md) explains each method, the optional hooks and the transfer rules.

## 4. Declare your limits and start the chip

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. The constructor stores the configuration and does no I/O. `Limits` has no defaults: your cell's highest charge
   voltage and current, and what your supply can deliver. `EnsureInitialized()` rejects a configuration without
   them (`InvalidArgument`) before it sends anything.
2. `EnsureInitialized()` calls your port's `EnsureInitialized()`, checks that DEVICE_ID reads `0x7C`, then reads the
   charger as found. Some pre-programmed options start with the charger on and the fast-charge timer off; if the
   charger is on and not covered by your limits the call returns `Unsafe` or `OutOfLimits`, or, with
   `AsFoundPolicy::MakeSafe`, switches charging off.
3. `ReadChargerStatus()` reads VBUS, the charger state (twice, to catch a change between reads) and the battery
   state, and classifies the fault. It writes nothing.

Nothing was written to the chip in this example. `Configure()` writes what `Config` asks for; `SetChargerMode()`
turns charging on. See the [API guide](api-guide.md).

This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `EnsureInitialized()` returns `InvalidArgument` | `Limits` is unset or a value is off the datasheet grid ([Configuration](configuration.md)) |
| `EnsureInitialized()` returns `NoDevice` | Power, SDA, SCL and the address: nothing acknowledged ([Troubleshooting](troubleshooting.md#ensureinitialized-returns-nodevice)) |
| `EnsureInitialized()` returns `Bus` | Your port's `EnsureInitialized()` or a bus fault |
| `EnsureInitialized()` returns `DeviceIdMismatch` | Another chip answered ([Troubleshooting](troubleshooting.md#ensureinitialized-returns-deviceidmismatch)) |
| `EnsureInitialized()` returns `Unsafe` or `OutOfLimits` | The charger was found on outside your limits ([Troubleshooting](troubleshooting.md#ensureinitialized-returns-unsafe-or-outoflimits)) |

## Next

- [API guide](api-guide.md): configure, charge, rails, interrupts, power states, errors.
- [Configuration](configuration.md): limits, acknowledgements, every block, Kconfig.
- [Examples](examples.md): the host example and the MCU examples.
- [Platforms](platforms.md): which MCUs have a port and an example.
