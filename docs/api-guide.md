# API guide

How to use the driver task by task. Every call returns `pf1550::Result<T>`: check it before you use `value`.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NXP Semiconductors PF1550 7:

    - SWx output voltage setting and DVS-done: 6.2.5 Output voltage setting in SWx, p. 28
    - LDO1 and LDO3: 7.2 LDO1 and LDO3 detailed description, p. 35
    - LDO load switch mode: 7.2.7 LDOy load switch mode, p. 38
    - Charger operating modes: 8.1 Operating modes and behavioral description, p. 43
    - Charger input source detection: 8.2 Charger input source detection, p. 45
    - Charger state diagram: 8.4 Charger state diagram, p. 46
    - Charger faults and restart: 8.9 Fault states, p. 64
    - Charger watchdog: 8.9.2 Watchdog timer state, p. 65
    - PWRON: 9.1 PWRON, p. 68
    - STANDBY pin: 9.2 STANDBY, p. 69
    - RESETBMCU: 9.3 RESETBMCU, p. 69
    - INTB and the interrupt registers: 9.4 INTB, p. 70
    - ONKEY: 9.6 ONKEY, p. 71
    - State machine and transitions: 10 PF1550 state machine, p. 72
    - System ON, OFF, SHIP and CORE_OFF: 10.2 System OFF states, p. 74
    - Regulator power-up sequencer: 10.6 Regulator power-up sequencer, p. 77
    - Regulator power-down sequencer: 10.7 Regulator power-down sequencer, p. 78
    - Device start-up timing and pre-programmed options: 11 Device start up, p. 78
    - Device ID, silicon revision and OTP flavour: 12.1 Specific PMIC Registers (Offset is 0x00), p. 80

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The shape of the API

| Group | Calls | Writes the chip |
| --- | --- | --- |
| Lifecycle | `EnsureInitialized()`, `IsInitialized()`, `GetConfig()`, `Configure()` | `Configure()` only |
| Identity and state | `ReadVariant()`, `ReadPowerState()`, `ReadSnapshot()`, `SelfCheck()`, `CheckChargerAgainstLimits()` | no |
| Charger | `ReadChargerStatus()`, `SetChargerMode()`, `RestartCharger()`, `ServiceChargerWatchdog()` | yes |
| Rails | `ReadRail()`, `SetRailModes()`, `SetRailVoltage()`, `EnableLoadSwitch()` | yes, except `ReadRail()` |
| Power states | `RequestStandby()`, `EnterShip()`, `EnterCoreOff()`, `CancelShutdownRequest()`, `ReadPowerDownSequence()`, `SetPowerDownSequence()` | yes, except the read |
| Interrupts | `SetIrqMask()`, `ReadCategories()`, `ReadSense()`, `ServiceInterrupts()`, `GetLatchedEvents()`, `ClearLatchedEvents()`, `IntbAsserted()` | masks and clears only |
| Bench | `ReadRegister()` | no, read only |

- **There is no raw register write.** `ReadRegister()` reads one documented register for bench work and writes
  nothing. The settings are typed (`Config` and its blocks), checked against the datasheet
  grids and your `Limits`, and every register written is read back ([Contract](contract.md#safety)).
- **Dangerous calls need a token.** `EnterShip(ShipConfirm{})`, `EnterCoreOff(CoreOffConfirm{})` and
  `EnableLoadSwitch(rail, LoadSwitchConfirm{.input_mv = 1800})` take a struct that you write out, so the call site says
  what it does; the load switch token carries the input voltage of the LDO, which the driver checks against `Limits`.
- Time is yours: the driver has no timer. The one wait it makes is bounded and goes through `DelayUs()`.

## Initialise and configure

```cpp title="Start and configure"
--8<-- "tests/docs/api_tasks.cpp:lifecycle"
```

`EnsureInitialized()` validates the stored `Config`, checks DEVICE_ID, checks your DVS declarations against Table 85
and reads the charger as found. It writes nothing, except that when `AsFoundPolicy::MakeSafe` is set and an unsafe
charge is found on, one write that puts the charger in linear-only mode. A start-up that fails on an unsafe charger
leaves the driver uninitialised, but `SetChargerMode(Off)` still works. `Configure()` writes every block whose `apply`
flag is true, in this order: the rails (in `Config::rail_order`), VSNVS, the coin cell, USBPHY, the LED, power
control, the power-down order, the interrupt masks and, last, the charger. A block with `apply = false` is left as the
chip holds it.

The driver does not know the chip's OTP option. Read it with `ReadVariant()` and decide what your board needs:

```cpp
const auto v = pmic.ReadVariant();
// v.value.option is Option::A1 .. A9, A0 (not programmed) or Unknown; v.value.flavor_raw is always the raw byte.
```

## Charge

The charger is parked before it is changed and is switched on only by the mode, written last. A configuration with
`charger.mode = ChargerMode::Charge` writes the parameters in linear-only mode and enables charging at the end, unless
the charger reports a fault (`ChargerFault`: the parameters are written, charging is not enabled).

```cpp title="Watch the charger"
--8<-- "tests/docs/api_tasks.cpp:charger"
```

- `ReadChargerStatus()` makes four reads: VBUS_SNS, CHG_SNS, BATT_SNS and CHG_SNS again (`consistent` says the two
  CHG_SNS reads agree). `fault` is classified: `None`, `TimerFault`, `ThermistorSuspend`, `ThermalShutdown`,
  `BatteryOvervoltage`, `BatteryOvercurrent`, `WatchdogExpired`, `BatteryMissing` or `UnknownState`. The reads are not
  atomic with each other (ambiguity C27).
- `SetChargerMode(Off)` and `LinearOnly` are allowed as soon as DEVICE_ID has passed, even after a failed start-up.
  `Charge` checks the charger's registers against `Limits`
  and the acknowledgements first, so a charge that was configured outside your limits by something else (a start-up
  option, another master) is refused with `OutOfLimits` or `Unsafe`.
- `RestartCharger()` leaves a latched fault state: off, verify, charge. Three attempts in a row without the charger
  reaching a charging state return `Refused`: find the cause (a cold or hot battery, a timer, a shorted cell).
- With `watchdog.enable` call `ServiceChargerWatchdog()` at least every 16 s. The datasheet reads 80 s or 32 s with
  20 % accuracy; 16 s covers every reading. The call reads BATFET_CNFG until two reads agree and refuses with `Unsafe`
  if the battery overcurrent protection reads as off without `allow_batfet_oc_off`: a stuck line reads zero. Whether
  WDTCLR holds what is written is open (C58); the driver writes it as an edge either way.
- The input limit check does not need `watchdog.enable`: a cable removal puts VBUS_LIN_ILIM back to its OTP value
  (Table 163). With the charger in Charge (or LinearOnly, when the charger block is applied) `ReadChargerStatus()`,
  `ServiceInterrupts()` (when a charger interrupt is set) and `ServiceChargerWatchdog()` all read the limit. A limit
  that is not the configured one is written again once VBUSIN is valid (while it is invalid the field is held in
  reset and nothing is written; `SelfCheck()` reports `input_limit_ok` false). A limit above `Limits` that cannot be
  restored (the charger block is not applied) switches the charger off and returns `OutOfLimits`. A failed check
  returns its error and not the status: an application that needs the status regardless calls `ReadSnapshot()`, which
  reads it without the check. The check is not written over a chip that reset meanwhile (`DeviceReset`), and a charger
  interrupt that `ServiceInterrupts()` read owes the check until one has passed: a call that fails after the status
  byte was read (a later pass, the clear write), or a `ClearLatchedEvents()` in between, does not drop it, and the
  next call makes it again.

## Rails

```cpp title="Change an LDO"
--8<-- "tests/docs/api_tasks.cpp:rails"
```

- Every rail needs a window in `Limits::rail[...]`; `SetRailVoltage()` and `SetRailModes()` return `OutOfLimits` for a
  rail without one or a voltage outside it.
- SW1 and SW2: the run voltage can be written only when you declared `Dvs::Enabled` in `BuckConfig::dvs`; otherwise
  `ReadOnlyRail`. The standby and sleep voltages are set in `Configure()` through `standby_uv` and `sleep_uv`. After a
  change of the run voltage of an enabled rail the call waits for the DVS-done flag (at most 256 us) and returns
  `Timeout` if it does not rise.
- SW3's voltage is read only (ambiguity C2): `SetRailVoltage(Rail::Sw3, ...)` returns `ReadOnlyRail`.
- `SetRailModes()` sets EN, STBY_EN, OMODE and LPWR. Low-power mode (`low_power`) needs `ack.load_below_10ma`: the
  datasheet specifies it for loads up to 10 mA.
- **`EnableLoadSwitch(Rail::Ldo1 or Ldo3, LoadSwitchConfirm{.input_mv})` is irreversible.** It turns an LDO into a
  switch: the output is the input voltage, with no current limit, and it cannot be undone until the next power cycle
  (LS_EN 1 to 0 is not allowed). `input_mv` is the voltage of the rail that feeds the LDO; it must lie inside the
  rail's `Limits` window, and the rail must not be in low-power mode (`Refused`).
- An enable (`SetRailModes()`, or a block in `Configure()`) first reads the voltage registers it brings into effect
  and checks them against `Limits` (`OutOfLimits`): a rail never comes on at a voltage the load does not tolerate.
  Standby and sleep need run (`InvalidArgument` otherwise): EN gates both (Table 42).
- `ReadRail()` reports the enables, the load-switch flag and the programmed run voltage, decoded in the DVS mode the
  part has (Table 85 for A1 to A3, else your declaration).

## Power states

- `ReadPowerState()` decodes STATE_INFO: `Wait`, `Run`, `Standby`, `Sleep`, `RegsDisable`, or `Unknown` (SHIP and
  CORE_OFF have no I2C-visible state because the chip is not powered, ambiguity C36).
- `RequestStandby(bool)` drives the STANDBY pin through your `GpioSet()`; `Unsupported` without it.
- `EnterShip()` and `EnterCoreOff()` are guarded:

```cpp title="Ship mode"
--8<-- "tests/docs/api_tasks.cpp:ship"
```

  `EnterShip()` returns `Refused` unless the chip is in Run, Standby or Sleep and VBUS reads invalid and below UVLO.
  Everything is cut, VSNVS included; recovery is a charger or a battery attach. The chip enters SHIP only with VBUSIN
  below 1.0 V, which the driver cannot measure (ambiguity C6): with VBUSIN between 1 V and UVLO it ends in
  REGS_DISABLE. `EnterCoreOff()` needs VBUS invalid and ONKEY not pushed, and arms GOTO_CORE_OFF: it takes effect
  at the next REGS_DISABLE, not at once. Both requests stay set in PWRCTRL3 until `CancelShutdownRequest()` clears
  them; call it when the request is no longer wanted. It is not refused after DEVICE_ID has passed.
- `ReadPowerDownSequence()` and `SetPowerDownSequence()` read and write the seven `*_PWRDN_SEQ` registers. The order
  decides which supply drops first, and a wrong order can damage a processor: the write needs
  `ack.power_down_order_reviewed`. All zero bypasses the sequencer (section 10.7).

## The sequencer

The power-up order is fixed by OTP and cannot be read back over I2C, so the driver reports it only for the
pre-programmed options A1 to A9 that Table 85 lists. The helpers are `constexpr` and have no I/O:

```cpp title="When does each rail turn on"
--8<-- "tests/docs/api_tasks.cpp:sequencer"
```

| Helper | Gives |
| --- | --- |
| `StartupProfileOf(Option)` | The `PWRUP_SEQ` order of each rail, the slot length (500 us for A1, else 2000 us) and whether PWRON is edge sensitive (A8); `InvalidArgument` for A0 and `Unknown` |
| `PowerUpDelayUs()`, `PowerUpDurationUs()` | When a rail starts, and when the last one does, from the start of the first slot |
| `ResetReleaseDelayMs()`, `ColdStartToResetReleaseUs()` | RESETBMCU's delay from `POR_DLY` (code 1 is undefined) and the whole cold-start time to its release |
| `PowerDownDelayUs()`, `PowerDownDurationUs()`, `IsPowerDownBypassed()` | The power-down schedule of a `PowerDownSequence`: `(highest - own)` steps of 2 ms |
| `MirrorOf(profile)` | The power-down order the registers hold after reset (section 10.7: it mirrors the power-up order) |

Where the datasheet is silent the readings are C54 to C56 in [Ambiguities](ambiguities.md).

## Interrupts

```cpp title="Service INTB"
--8<-- "tests/docs/api_tasks.cpp:interrupts"
```

- 31 sources in 8 groups (`IrqGroup`): charger, SW low side, SW high side, SW DVS, LDO, temperature, ONKEY and
  miscellaneous. `Has(events, IrqSource::X)` tests one; `IsFaultSource()` and `fault_class` mark rail, die and
  system-overvoltage faults.
- A source is masked by default (`IrqMask{}` masks all). `Config::apply_irq_mask` writes `Config::irq_mask`;
  `UnmaskOnly(std::array{...})` builds one from a list of sources. A masked source still sets its status bit, but
  does not drive INTB.
- `ServiceInterrupts()` copies each status byte out before it clears it (read-then-clear, at most three passes). Its
  result holds the bytes as read, plus the status bits that other calls read in the meantime (the driver latches them;
  `GetLatchedEvents()` shows them). With `GpioGet()`, INTB high, every source unmasked through `SetIrqMask()` and an
  empty latch it returns at once without a transfer; otherwise it reads the chip. PWRUP_I is reported like any other
  event (it is the standby wake-up flag, not a reset flag).
- `ReadCategories()` and `ReadSense()` are the live views (not latched): use them to poll without clearing.
- A DVS step that the driver waits for (`SetRailVoltage()`, `Configure()` of SW1 or SW2) reads SW_INT_STAT2 and clears
  the awaited rail's DVS-done bit, so that rail's own event is consumed by the wait and never reaches the latch; the
  other rail's bit is kept in the latch. Use `ReadSense(IrqGroup::SwDvs)` to watch DVS done without consuming it.

## Diagnostics

- `ReadSnapshot()` reads about 60 registers: identity, state, the charger, every sense register, the rails, the
  power-down order and `POR_DLY`. It is for logs and bring-up, not for a control loop.
- `SelfCheck()` answers four questions without writing: DEVICE_ID, state Run, every configured rail enabled as
  configured, and no latched fault.
- `CheckChargerAgainstLimits()` repeats the as-found check at any time.

## Handle errors

```cpp title="Explain an error"
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| Error | Usually means | Do |
| --- | --- | --- |
| `NotInitialized` | `EnsureInitialized()` has not succeeded | Call it, and check its error |
| `InvalidArgument` | Off the grid, reserved, or `Limits` unset | Fix the value; nothing was sent |
| `OutOfLimits` | Valid for the chip, outside your `Limits` | Fix the value or, if the hardware allows it, the limit |
| `Unsafe` | A downgrade without its acknowledgement | Set the flag in `Config::ack` only if you mean it |
| `ReadOnlyRail` | SW3, or a buck not declared DVS | Declare `Dvs::Enabled` if the OTP says so |
| `Refused` | A precondition of SHIP, CORE_OFF or a restart limit | Read the state with `ReadPowerState()` and `ReadChargerStatus()` |
| `ChargerFault` | The charger reports a fault | Read `ReadChargerStatus()`; `RestartCharger()` after the cause is gone |
| `VerifyFailed` | A register read back differently, or a read that is not stable or reads all ones | Retry once; if it repeats, suspect a noisy line, a reset or another master |
| `DeviceReset` | A register the driver changed is back at its start-up value: the chip reset | `EnsureInitialized()` and `Configure()`, see [Troubleshooting](troubleshooting.md) |
| `Timeout` | DVS-done did not rise | Check the load and the rail's enable; the step may be slow under a heavy load |
| `NoDevice` | Nothing acknowledged the address | Power, SDA, SCL, the address ([Troubleshooting](troubleshooting.md)) |
| `Bus`, `DeviceIdMismatch` | A bus fault; another chip | [Troubleshooting](troubleshooting.md) |
| `Unsupported` | An optional hook is missing | Add `GpioSet()` or `GpioGet()` to your port |

## Shut down

There is no `Deinitialize()`: the chip is a supply, and the driver never switches it off. To stop using it, drop the
object. Use `RequestStandby()`, `EnterShip()` or `EnterCoreOff()` to change the chip's own state.
