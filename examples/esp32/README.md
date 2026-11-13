# PF1550 ESP32 examples

ESP-IDF applications that run the PF1550 driver on an ESP32-C6, built by CI on every pull request.

## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | PF1550 | Notes |
| --- | --- | --- |
| GPIO6 | SDA | I2C0, 400 kHz; 2.2 kΩ pull-ups to the I2C supply |
| GPIO7 | SCL | |
| GPIO10 | STANDBY | Optional; set `standby = GPIO_NUM_NC` in `Esp32Pf1550Pins` when wired low |
| GPIO11 | INTB | Open drain, active low; optional, set `intb = GPIO_NUM_NC` when not wired |
| GND | GND | Common ground with the PF1550 board |

The chip answers at address `0x08` unless its OTP set another (`0x08` to `0x0F`); pass the address as the third
constructor argument.

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh pf1550_basic_example Debug
./scripts/flash_app.sh flash_monitor pf1550_basic_example Debug
```

`idf.py menuconfig` shows "PF1550 power management IC" under Component config: the cell and supply limits that
`pf1550::KconfigDefaults()` returns. The limits are the safety envelope of the application: set them from the
cell's and the supply's data sheets before the first run.

## Expected output

```text
I (312) pf1550_example: PF1550 driver 0.1.0
I (318) pf1550_example: initialised, option 2
I (342) pf1550_example: configured
I (820) pf1550_example: charger state 3, fault 0, LDO1 1800000 uV
```

`EnsureInitialized failed: NoDevice` means nothing acknowledged the address: check power, SDA and SCL and the
address. `DeviceIdMismatch` means another chip answered. `Configure failed: Unsafe` or `OutOfLimits` means the
charger or a rail was found or asked outside the limits; read `docs/contract.md`.

## Files

| File | What it is |
| --- | --- |
| [`main/pf1550_basic_example.cpp`](main/pf1550_basic_example.cpp) | Configure, charge and log |
| [`main/pf1550_interrupt_example.cpp`](main/pf1550_interrupt_example.cpp) | INTB events and the watchdog |
| [`main/esp32_pf1550_port.hpp`](main/esp32_pf1550_port.hpp) | The ESP-IDF port: bus, STANDBY, INTB, failure report, delay, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
