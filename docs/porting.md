# Porting

Write the one class that connects the driver to your MCU's I2C bus, and optionally to two pins.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NXP Semiconductors PF1550 7:

    - STANDBY pin: 9.2 STANDBY, p. 69
    - INTB and the interrupt registers: 9.4 INTB, p. 70
    - I2C address, clock and register framing: 9.7 Control interface I2C block description, p. 71
    - I2C operation: single-register transfers: 9.7.2 I2C operation, p. 72

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `pf1550::I2cInterface<Self>` (in
[`inc/pf1550_i2c_interface.hpp`](../inc/pf1550_i2c_interface.hpp)). The base uses CRTP and declares no hook
itself; the driver template is constrained by the `pf1550::I2cPort` concept, so calls are resolved at compile
time (no virtual functions, nothing to allocate) and a missing or misdeclared hook is a compile error at the point
of use.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept` | yes | START, `addr` + W, `len` bytes (register, then one data byte), STOP. `false` on a NACK or any bus error |
| `bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx, std::size_t rx_len) noexcept` | yes | START, `addr` + W, `tx` (the register), repeated START, `addr` + R, `rx_len` (1) byte, STOP |
| `void DelayUs(uint32_t us) noexcept` | yes, for the DVS wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive STANDBY; `false` if the pin write failed. Without it `RequestStandby()` returns `Unsupported` |
| `bool GpioGet(CtrlPin, bool& asserted) noexcept` | no | Read INTB; `asserted` is true when the pin is low. Without it `ServiceInterrupts()` always reads the chip |
| `bool GetLastFailure(I2cFailure& why) noexcept` | no | What the last failed transfer was: `AddressNack`, `DataNack` or `Bus`. Without it the driver cannot tell an absent chip from a bus fault |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

`addr` is the 7-bit address the driver was constructed with; shift it yourself if your SDK wants the 8-bit form.

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

## Bus requirements

The driver states what the chip needs as `constexpr` values; check your port against them with `static_assert`.

| Constant | Value | Why |
| --- | --- | --- |
| `kI2cAddress` | `0x08` | Default 7-bit address; OTP sets `0x08` to `0x0F` |
| `kMaxClockHz` | 400 000 | Fast mode (section 9.7.2) |
| `kRepeatedStart` | `true` | A register read is a combined write-read with a repeated START; the port must not send a STOP between them |
| `kMinEdgeNs` | 20 | Shortest SCL and SDA rise and fall time the chip accepts |

- Transfers are one register at a time: the datasheet does not describe auto-increment (ambiguity C48), so the port
  never sees a burst of more than one data byte.
- **Buffers.** The pointers the driver hands to `Write()` and `WriteRead()` point into the driver object (the register
  bus owns a one-byte read buffer, the register-address byte and a two-byte write frame; no heap, never the caller's
  stack), so a DMA that reads or fills them reaches them. A port that needs DMA-capable memory places the whole
  `PF1550` object there.
- The pull-ups on SDA and SCL go to VDDIO (1.7 V to 3.3 V). Size them for 400 kHz and your bus capacitance.
- An address NACK is the signature of an absent or unpowered chip. Implement `GetLastFailure()` if your SDK can tell
  it from a data NACK; the driver then reports `NoDevice` instead of `Bus`.

## Control pins

- **STANDBY** (`CtrlPin::Standby`, output): `GpioSignal::ACTIVE` means "standby requested". With `STANDBYINV` = 0, the
  reset value, that is a high level. Drive the pin to its idle level (low) before you enable the output, so
  configuring it never requests STANDBY.
- **INTB** (`CtrlPin::Intb`, input): open drain, active low. `asserted` is true when the pin reads low. Add a pull-up
  to VDDIO on the board.

## Optional hooks

A hook that is declared must have exactly the signature above, including `noexcept`: a hook with a wrong signature is
a compile error, never silently ignored. A hook that is absent is detected at compile time and its calls return
`Unsupported` or log nothing.

```cpp title="A port with every hook"
--8<-- "tests/docs/ports.cpp:full-port"
```

```cpp title="Using STANDBY"
--8<-- "tests/docs/ports.cpp:standby"
```

A `GpioSet()` that returns `void` is a compile error: the driver must see a failed pin write. The
[compile-fail tests](testing.md) check this rule, the `DelayUs()` signature, the `noexcept` rule and a configuration
without limits.

## Reference implementations

Every port lives with its example, in `examples/<platform>/`; the driver has none. [Platforms](platforms.md) lists
them and how to add one.

- [`examples/host/host_pf1550_port.hpp`](../examples/host/host_pf1550_port.hpp): any PC, against a simulated
  chip. Start from it for a host build or a test bench; it is also the smallest complete port.
- [`examples/esp32/main/esp32_pf1550_port.hpp`](../examples/esp32/main/esp32_pf1550_port.hpp): ESP-IDF, with
  every hook. Start from it for any ESP-IDF target.

## Check your port

1. Build it: the ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_port.hpp),
   with the driver instantiated over them.
2. Call `EnsureInitialized()`: `Ok` proves that writes and reads work and that DEVICE_ID read `0x7C`. `NoDevice` or
   `Bus` means the wiring or the address is wrong.
3. Call `ReadChargerStatus()` and `ReadPowerState()`: they read only. With a supply on VBUSIN the VBUS flags change.
4. If you added `GpioGet()`: `IntbAsserted()` follows INTB. If you added `GpioSet()`: probe the STANDBY pin while
   calling `RequestStandby()`.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex. When
several chips share a bus, the port must also serialise the bus. See [Contract](contract.md#threading).

## Editor setup

The clang-format and clang-tidy configuration is in `_config/`, and the repository root has no copy of it. Tools
that take a path use it directly; CI does the same:

| Tool | Setting |
| --- | --- |
| Command line | `clang-format --style=file:_config/.clang-format`, `clang-tidy --config-file=_config/.clang-tidy -p build` |
| VS Code (C/C++ extension) | `"C_Cpp.clang_format_style": "file:${workspaceFolder}/_config/.clang-format"` and `"C_Cpp.codeAnalysis.clangTidy.args": ["--config-file=${workspaceFolder}/_config/.clang-tidy"]` |
| clangd (VS Code clangd, Neovim, Emacs), CLion | These find `.clang-format` and `.clang-tidy` only by name, from the file upwards: copy both to the root once (`cp _config/.clang-format _config/.clang-tidy .`). `.gitignore` lists the copies, so they never reach a commit; copy again after `_config/` changes |

Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (clangd: `--compile-commands-dir=build`) so the editor sees the
same flags as the build.
