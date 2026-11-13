# Contract

What the PF1550 driver promises, what it needs, and the rules a caller follows.

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it. A change to anything here is a breaking change, listed under
**Breaking** in the [changelog](changelog.md).

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NXP Semiconductors PF1550 7:

    - Electrical limits and supply ranges: 5.3 Electrical characteristics, p. 12
    - SW1 and SW2: DVS and non-DVS operation: 6.2.2 SWx DVS and non-DVS operation, p. 26
    - LDO load switch mode: 7.2.7 LDOy load switch mode, p. 38
    - Battery overcurrent limit and BATFET: 8.9.6 Battery overcurrent limit, p. 66
    - System ON, OFF, SHIP and CORE_OFF: 10.2 System OFF states, p. 74
    - State transition conditions (Table 83): 10.5 State diagram and transition conditions, p. 76
    - Regulator power-down sequencer: 10.7 Regulator power-down sequencer, p. 78
    - Register map and access types: 12 Register map, p. 80
    - Register OTP bitmap: 12.5 Register OTP bitmap, p. 128

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Promises

### Lifecycle

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` is the only way to start the driver. It validates the stored `Config` first
  (`InvalidArgument`, `OutOfLimits` or `Unsafe`, nothing sent), brings the bus up, checks that DEVICE_ID reads `0x7C`,
  checks your DVS declaration against Table 85 and checks the charger as
  found against your `Limits`. Once it has succeeded, later calls return `Ok` with no I/O. After a failure the driver
  stays uninitialised and the call can be repeated.
- Every call that touches the chip returns `Error::NotInitialized` before `EnsureInitialized()` succeeds, and sends
  nothing. No call starts or resets the chip as a side effect. Some calls work as soon as DEVICE_ID has passed, even
  when start-up then failed (an unsafe charger found on, for example): `SetChargerMode(Off)` or `LinearOnly`,
  `SetRailModes()` with every state off, the off step of `RestartCharger()`, `CheckChargerAgainstLimits()` and
  `CancelShutdownRequest()`. An unsafe charger can always be switched off.
- The chip can reset in the middle of a session (UVDET, a watchdog, a REGS_DISABLE pulse): it then starts in its OTP
  state, which may be a charger on with no timer, and the driver's `Configure()` is gone. The datasheet defines no reset
  flag (PWRUP_I is the STANDBY or SLEEP to RUN completion flag, Table 108, ambiguity C59), so the driver remembers the
  bits it changed and verified (at most four registers, the newest ones) and every call that switches something on or
  changes the chip reads those registers first (up to four extra reads); a difference twice in a row returns
  `DeviceReset`, writes nothing, and leaves the driver uninitialised until `EnsureInitialized()` and `Configure()` run
  again. `SelfCheck()` reports it the same way, and `Configure()` looks once more before it writes the charger block. A
  rail the driver switched off is remembered as off, so a reset that brings it back on is seen. A reset that leaves
  every bit the driver wrote at its start-up value is not seen, and does no harm; a reset in the middle of a call that
  writes several registers is seen by the next check, not by the writes in between. `EnsureInitialized()` on a driver
  that already is initialised returns at once and checks nothing: to look for a reset, call `SelfCheck()`.
- A cable removal is not a chip reset. `VBUS_LIN_ILIM` goes back to its OTP value when VBUSIN becomes invalid
  (Table 163, reset condition CHGPOK_RSTB, ambiguity C14), so it is never kept as evidence of a reset. The charger
  keeps its mode through a cable removal, so a charger that was charging goes on charging at the OTP limit until the
  next watchdog service: `ServiceChargerWatchdog()` reads the limit while the charger is in Charge, writes the
  configured one again when the charger block is applied (a warning is logged), and otherwise switches the charger off
  and returns `OutOfLimits` when the limit is above `Limits.input_max_ma`. `SelfCheck()` reports it as
  `input_limit_ok`. Service the watchdog at least every `kWatchdogServiceMaxS` seconds (16 s) to bound that window.
  A charge start refused with `OutOfLimits` or `Unsafe` leaves the charger off, also when it was found charging.
- A switch-off is never refused for a reset report or a failed re-start: `SetRailModes()` with every state off,
  `SetChargerMode(Off or LinearOnly)`, the off step of `RestartCharger()` and `CancelShutdownRequest()` need only a
  chip that answered with DEVICE_ID once (a bus error during a re-start does not take that back; another device
  answering does), and the write goes out even when the register cannot be read. The bits other than the switched ones
  then go out as zero: for CHG_OPER that is also WDTEN and an acknowledged DISBATFET, which the application sets again
  with `Configure()`. Only the switched bits are read back.
- `InvalidArgument`, `ReadOnlyRail`, `Unsupported` and the `OutOfLimits` or `Unsafe` that come from the arguments and
  the `Config` mean nothing was sent: they are decided before the reset check reads the chip. An `OutOfLimits`,
  `Unsafe` or `Refused` found by reading the chip (the as-found charger, a held voltage, a protection that reads off,
  the shutdown preconditions) comes after reads and before any write.
- Every port failure propagates: a transfer that returns `false` is `Error::Bus`, or `Error::NoDevice` when your
  port's `GetLastFailure()` says the address was not acknowledged.

### Safety

- There is no public raw register write. Every write is a typed call, goes through one private function that refuses
  any register the datasheet does not document as writable (read-only, status, key and undocumented registers), and
  is a read-modify-write that preserves reserved bits. The typed setter reaches configuration and mask registers
  only; status registers are cleared by the interrupt service and PWRCTRL3 is written by the shutdown calls.
- A value that is written back must be one the chip holds. The read of a read-modify-write is repeated until two
  reads agree (at most three), and a register other than an interrupt mask that reads all ones is `VerifyFailed`
  (a floating or stuck line). `ServiceChargerWatchdog()` refuses with `Unsafe` when BATFET_CNFG reads with the battery
  overcurrent protection off and `allow_batfet_oc_off` is not set: a stuck line reads zero.
- `Limits` has no defaults. A configuration without `cell_max_mv`, `cell_max_charge_ma` and `input_max_ma` is rejected.
  A rail the application did not give a window is never enabled or set by the driver (`OutOfLimits`).
- The charge voltage and current written never exceed `Limits`, and the input current limit never exceeds
  `input_max_ma`. `Limits::coin_cell_max_mv` must be at least 3300 mV for a coin cell charge voltage above 1.8 V
  (ambiguity C40).
- A safety downgrade is accepted only with its acknowledgement in `Config::ack`, so that it is visible in the
  application's source: no fast-charge timer, thermistor ignored, BATFET kept closed on a fault, battery overcurrent
  protection reduced or off, 5 mA or 10 mA termination, a low-power mode on a rail, a coin cell charger, a changed
  hard-reset path, a changed undervoltage threshold, DISBATFET, a changed power-down order.
- Every typed write is read back, and the whole byte is compared (bits the chip changes by itself, such as the
  watchdog clear bits, are excluded). A register that reads back differently is `VerifyFailed`; the value is not
  trusted. A rail enable that the chip clears on its own after an overcurrent (`REGSCPEN`, Table 136) reads back
  differently and reports `VerifyFailed` as well. This deviates from the handbook's per-call `Verify` argument (see
  Deviations). Not read back: the watchdog service's WDTCLR bits, the interrupt status clears and PWRCTRL3.
- A rail is enabled only at a voltage inside its `Limits` window: the voltage registers that the enable brings into
  effect and the call does not write are read and decoded first (`OutOfLimits`). The same applies to `Configure()`.
- The charger is parked before it is changed: `Configure()` writes CHG_OPER to "linear regulator only", then every
  charger parameter, then the watchdog and BATFET bits, and the mode last. Charging is enabled only by a charger
  block in `Configure()` that asks for `Charge`, or by `SetChargerMode(Charge)`. `SetChargerMode(Charge)` re-checks
  the charger's registers against `Limits` and refuses with `ChargerFault` while the charger reports a fault.
- The as-found check runs in `EnsureInitialized()`. With `AsFoundPolicy::Report` (the default) an unsafe charger found
  on leaves the driver uninitialised and writes nothing; with `MakeSafe` the driver writes CHG_OPER to "linear only"
  and checks again. A charger found off or in linear-only mode is not checked.
- `EnterShip()` needs a `ShipConfirm`, state Run, Standby or Sleep, VBUS not valid and below UVLO. `EnterCoreOff()`
  needs a `CoreOffConfirm`, VBUS not valid and ONKEY not pushed. Otherwise `Refused`, with nothing written. The driver
  cannot measure VBUSIN below 1.0 V, which SHIP needs (ambiguity C6): with VBUS between 1 V and UVLO the chip ends in
  REGS_DISABLE instead of SHIP.
- GOTO_SHIP and GOTO_CORE_OFF are plain read/write bits that stay set (Table 138). A request is armed, not a
  command that is over once it is sent: GOTO_CORE_OFF acts only when the chip is in REGS_DISABLE (VBUS invalid, ONKEY
  high; Table 83 footnotes 8 and 9, section 10.2.2), so in Run, Standby or Sleep nothing happens and the bit stays
  armed until the next REGS_DISABLE. A request that did not end in SHIP can fire at a later charger attach or battery
  cycle. `CancelShutdownRequest()` clears both bits and reads PWRCTRL3 back; it is not refused after DEVICE_ID has
  passed. Each request writes one command bit and clears the other, so a bad read cannot turn SHIP into CORE_OFF.
- `EnableLoadSwitch()` is irreversible: the chip cannot leave the mode until the next power cycle (LS_EN 1 to 0 is not
  allowed, Tables 130 and 134), and the output is the input rail voltage with no current limit (7.2.7). It needs a
  `LoadSwitchConfirm{.input_mv}` with the input voltage of the LDO, which must lie inside the rail's `Limits`
  window, and refuses (`Refused`) while the rail is in low-power mode.
- `SetPowerDownSequence()` and the `PowerDownConfig` block need `ack.power_down_order_reviewed`.
- Reserved bits and reserved codes are never written; a code the datasheet marks reserved is not encodable
  (`InvalidArgument`).

### Timing and waits

- Every wait is bounded. The only wait is the DVS-done wait of `SetRailVoltage()`: at most `kDvsWaitMaxUs` (256 us),
  polled in 16 us steps through `DelayUs()`, and only when the run voltage changes and the rail is enabled
  (`Timeout` otherwise).
- The charger watchdog, when `WatchdogConfig::enable` is set, must be serviced by the application every
  `kWatchdogServiceMaxS` (16 s) or less (the datasheet reads 80 s or 32 s with 20 % accuracy, ambiguity C8). The
  driver has no timer: `ServiceChargerWatchdog()` is yours to call. The input limit check after a cable removal does
  not depend on it: `ReadChargerStatus()` and `ServiceInterrupts()` (on a charger interrupt, also one read by a call
  that failed afterwards) run it too, with the charger in Charge (or LinearOnly, when the charger block is applied).
  While VBUSIN is invalid the limit field is held in reset: the restore waits for a valid supply and `SelfCheck()`
  reports `input_limit_ok` false meanwhile.
  After an unseen chip reset the status and interrupt paths write nothing and return `DeviceReset`: a charger that
  the reset left at an OTP limit above `Limits` stays in that OTP state until `EnsureInitialized()`, whose as-found
  check handles it (a switch-off by `SetChargerMode(Off)` is never refused).
- `RestartCharger()` makes at most `kMaxChargerRestarts` (3) attempts in a row without the charger reaching a charging
  state and then returns `Refused`.

### Interrupts

- `ServiceInterrupts()` reads INT_CATEGORY, copies each flagged group's status byte out, then writes the same bits
  back to clear them, at most three passes. A bit that sets between the copy and the clear is not lost: it is read in
  the next pass or the next call. The event bytes are returned as read, before the clear.
- The status registers are "Read or Write 1 to clear" (Tables 90 to 110, C3): a read by another call may clear a bit.
  The driver keeps every status bit that any of its calls reads (`ReadRegister()`, `SelfCheck()`, the reset check, the
  DVS wait) in a latch; `ServiceInterrupts()` reports them with the events and empties the latch, and
  `GetLatchedEvents()` and `ClearLatchedEvents()` give access without bus traffic.
- With `GpioGet()` and INTB not asserted the call returns an empty result without bus traffic, but only when the
  latch is empty, `SetIrqMask()` left no source masked (a masked source does not pull INTB, and its status bit
  still sets) and no input limit check is pending (one is owed from the moment a charger status byte was read, by
  the service or by `ReadRegister()`, until a service has passed it). With the power-up mask (every source masked)
  the call reads the chip.
- Residual risk: a status bit is one flag. An event of the same source that occurs again between the status read
  and the clear write is merged with the first and reported once, in either reading of C3 (write-one-to-clear,
  clear-on-read, or both). An event of another source in the same group is not lost: only the bits that were read
  are written back. Not verified on hardware.
- The DVS-done wait of `SetRailVoltage()` polls and clears SW_INT_STAT2; the interrupt service clears the same
  register. Use one of them for the DVS-done bits of a rail: a call to `ServiceInterrupts()` from another task
  during a voltage step can swallow the event, and the wait then returns `Timeout`.
- The status bytes of the charger and the PMIC groups are not atomic with each other; `ChargerStatus::consistent`
  says whether CHG_SNS read twice with equal values (ambiguity C27).

### Data

- `Config{}` holds no limits and is rejected; `SingleCellLimits()` gives a starting point (4.2 V, 500 mA, 1 A) that the
  application must check against its own cell and supply.
- `ReadVariant()` never returns an error for an option the datasheet does not list: it reports the raw bytes and
  `Option::Unknown`. **A0 parts** ("not programmed") are supported by reading only: the driver assumes nothing about
  their start-up values, `StartupProfileOf(Option::A0)` is `InvalidArgument`, and the as-found check still applies.
- `SILICON_REV` is stored raw; the driver makes no decision on it (ambiguity C22).
- The DVS mode of SW1 and SW2 is an OTP fuse (`OTP_SWx_DVS_SEL`) that no register reports (ambiguity C1). You declare
  it in `BuckConfig::dvs`. The driver writes a buck's voltage registers only when you declared `Dvs::Enabled`, and
  returns `ReadOnlyRail` otherwise. SW3 is never written. For the listed options A1 to A3 the declaration is checked
  against Table 85 in `EnsureInitialized()`: a block you configure with the wrong mode is `Refused` (a rail written in
  the wrong mode ends at 3.3 V, Table 31), and the part's mode decides how an unconfigured buck is read. For a part
  Table 85 does not list, DVS needs `ack.dvs_on_unlisted_part` (`Unsafe` without it, ambiguity C57).
- The OTP image is not read back: the public datasheet documents no read window (ambiguity C5). `docs/datasheet.md`
  says so under "What is not covered".

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `pf1550_version.h` |
| Your port | | The bus, within the bus requirements; the delay (the DVS-done wait needs `DelayUs()`); optionally STANDBY, INTB, a failure report and logging ([Porting](porting.md)) |

Nothing else: no RTOS, no SDK, no other repository of the org.

## Threading

The driver is not thread-safe and takes no lock. Use each driver object from one task at a time, or guard every
call on it with one mutex; the layer that owns the driver owns that lock, and the bus lock when several chips
share a bus. Nothing may run in an ISR: every call can block for a bus transfer or a delay. Separate objects
share no state.

## Errors

`pf1550::Error` inside `pf1550::Result<T>`. Values 0 to 9 are the shared base set; the chip-specific values start
at 16 and each one is returned by some call (`tests/test_errors.cpp` checks it).

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | `EnsureInitialized()` has not succeeded |
| `InvalidArgument` | 2 | A value is off the datasheet grid, reserved, not an enumerator, or `Limits` is unset; nothing was sent |
| `Bus` | 3 | A port hook returned `false` |
| `Timeout` | 4 | The DVS-done flag did not rise within `kDvsWaitMaxUs` |
| `DeviceIdMismatch` | 5 | DEVICE_ID is not `0x7C` |
| `Crc` | 6 | Not returned: I2C transfers carry no CRC |
| `Busy` | 7 | Not returned: the chip has no busy state |
| `DeviceFault` | 8 | Not returned: faults are reported as `ChargerFault` or in the status types |
| `Unsupported` | 9 | The optional port hook the call needs is absent (`GpioSet()`, `GpioGet()`) |
| `OutOfLimits` | 16 | A valid value outside your `Limits`, or a charger found on outside them; nothing was sent |
| `ReadOnlyRail` | 17 | SW3, or SW1 or SW2 not declared `Dvs::Enabled`: its voltage is not written |
| `Unsafe` | 18 | A safety downgrade without its acknowledgement; nothing was sent |
| `Refused` | 19 | A precondition of a dangerous call is not met (state, VBUS, ONKEY, restart count) |
| `ChargerFault` | 20 | The charger reports a fault state; charging is not enabled |
| `VerifyFailed` | 21 | A register read back differently from what was just written |
| `NoDevice` | 22 | Nothing acknowledged the address (needs the optional `GetLastFailure()` hook) |
| `DeviceReset` | 23 | A register the driver changed reads at its start-up value again: the chip reset in the session; run `EnsureInitialized()` and `Configure()` |

Three base values (`Crc`, `Busy`, `DeviceFault`) are never returned: the chip's bus has no CRC, no busy state and
no fault bit that the driver reports as an error. They stay in the enumeration for the shared base set.
The [API guide](api-guide.md#handle-errors) says what to do about each.

## Memory

No heap, no exceptions, no RTTI, no `std::function`, no `std::string`, no `printf` and no static mutable state. A
driver object holds a reference to the port, the `Config` (296 bytes), the status latch (eight bytes) and a few flags:
336 bytes on a 64-bit host. The
largest stack objects are the diagnostic `Snapshot` (120 bytes) and the `Config` copies you make yourself. A
transfer moves one data byte, so no buffer is larger than two bytes.

## Platform

Platform-free: the same code runs on any target with a C++20 compiler, including the host tests. The ESP-IDF port
in [`examples/esp32`](examples.md) is an example, not part of the driver.

## Deviations from the standard

| Rule | Deviation | Why |
| --- | --- | --- |
| KB-ARCH-041 (read-back only on request, per call) | Every typed write is followed by a read-back and `VerifyFailed` is returned on a mismatch | The chip controls a battery charger and the supply of a processor. The datasheet describes registers whose writes are loaded from OTP, reset by events that the I2C interface does not report and "write-1-to-set" (ambiguity C4). A write that did not take effect cannot be allowed to pass silently. The cost is one extra single-register transfer per register written. |
| Optional hooks by `requires` | None: the driver detects them with named concepts as the standard asks | |
| MISRA C++:2023 rules 6.9.2, 15.1.4 and 0.0.2 (CodeQL advisories, expected) | Rule 6.9.2: the parameter of the `_u8`, `_u16` and `_u32` literal operators is `unsigned long long`, the type the language fixes. Rule 15.1.4: the `PF1550` constructor leaves the register bus without a default member initialiser (it holds a reference). Rule 0.0.2: cppcheck's `knownConditionTrueFalse` on a port whose hooks are constants | Not code defects; no waiver is recorded (see below) |

Deviations are proposed here, not waived: `.kokeb/handbook.yml` carries no waiver and `docs/deviations.yml` does not
exist for this driver. The maintainer decides whether the read-back becomes a waiver (the entry would name rule
KB-ARCH-041, this page as the place, and the one-transfer cost) or a per-call argument. No typed write targets a
write-1-to-clear or self-clearing bit: the typed setter refuses status and command registers.

## Open questions

[Ambiguities](ambiguities.md) lists every place where the datasheet is silent or inconsistent (C1 to C59), the
reading the driver takes, and the model knob that runs the other reading. Nothing in the driver has been verified on
hardware; each bench step that would settle a row is named in [Datasheet map](datasheet.md#what-to-verify-on-the-bench).
