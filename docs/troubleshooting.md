# Troubleshooting

Symptoms, causes and fixes, cheapest check first. The driver never retries and never hides a failure: each symptom is
an `Error` or a status value you can read.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NXP Semiconductors PF1550 7:

    - Charger faults and restart: 8.9 Fault states, p. 64
    - Charger watchdog: 8.9.2 Watchdog timer state, p. 65
    - Device ID, silicon revision and OTP flavour: 12.1 Specific PMIC Registers (Offset is 0x00), p. 80

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## EnsureInitialized returns InvalidArgument

The configuration was rejected before anything was sent.

1. `Limits` is unset: `cell_max_mv`, `cell_max_charge_ma` and `input_max_ma` have no defaults.
2. A value is off the datasheet grid: charge voltage in 20 mV steps from 3500 to 4440, charge current in 50 mA steps
   from 100 to 1000, one of 21 input limits, a rail voltage on its table.
3. Call `pf1550::Validate(config)` on your own: it returns the same error without the bus. `OutOfLimits`, `Unsafe` and
   `ReadOnlyRail` come from the same function and tell you which rule failed ([Configuration](configuration.md)).

## EnsureInitialized returns NoDevice

Nothing acknowledged the address. This needs the optional `GetLastFailure()` hook; without it the same condition is
`Bus`.

1. Is the chip powered (VSYS, VBUSIN or a battery) and is VDDIO present?
2. Is the address right? OTP sets it to `0x08` to `0x0F`; pass it as the third constructor argument. Scan the bus
   with a logic analyser: which address acknowledges?
3. Are SDA and SCL swapped, or missing their pull-ups to VDDIO?

## EnsureInitialized returns Bus

The port reported a failure: either `EnsureInitialized()` returned `false` or a transfer did.

1. Check that your port's `EnsureInitialized()` returns `true` on its own (log it).
2. Check the bus pins and the clock (400 kHz at most) in your port against the board.
3. With a logic analyser, check that the first frame (a read of register `0x00`) appears on the bus and that the read
   is a write, a repeated START and a read, not a STOP in between.

## EnsureInitialized returns DeviceIdMismatch

The chip answered, but DEVICE_ID is not `0x7C`.

1. Is another device at that address? Remove the other devices from the bus.
2. A read that returns `0x00` or `0xFF` is a floating or stuck line: check the pull-ups and the wiring. The driver
   reports it as `Bus`, not `DeviceIdMismatch`, and keeps a chip it identified before as identified, so a switch-off
   still goes out.
3. A part of another family, or one that is not a PF1550, will not read `0x7C`.

## EnsureInitialized returns Unsafe or OutOfLimits

The charger was found on, and what it holds is not covered by your `Limits` and acknowledgements (or the battery path
was found forced off, DISBATFET). Some pre-programmed options start with the charger on and the fast-charge timer off
(Table 85), so this is the check doing its job. The driver stays uninitialised, and the charger may still be charging.

1. Switch it off first: `SetChargerMode(ChargerMode::Off)` (or `LinearOnly`) works after a failed start-up, as soon as
   DEVICE_ID has passed. `CheckChargerAgainstLimits()` works too: it returns what was found (`ChargerAsFound`: mode,
   voltage, current, input limit, timer, thermistor, battery overcurrent, DISBATFET) together with the error.
2. Either raise the limit or set the acknowledgement if your hardware really allows what the chip holds, or set
   `as_found_policy = AsFoundPolicy::MakeSafe` so the driver switches charging off (linear regulator only) and checks
   again.
3. Run `EnsureInitialized()` again. `Configure()` then writes your charger settings, and `SetChargerMode(Charge)`
   enables charging.

## A call returns DeviceReset

The chip reset in the middle of the session (UVDET, a watchdog, a REGS_DISABLE pulse): a register the driver changed
reads at its start-up value again (the driver compares the bits it changed with what it wrote; PWRUP_I is no reset
flag, it also rises at every standby wake-up). The charger is back in its OTP state, which may be on with no timer,
and nothing that `Configure()` wrote is there. Nothing was written by the call that reported it.

1. Call `EnsureInitialized()` and `Configure()` again; the as-found check runs again.
2. `GetLatchedEvents()` still holds the status bits that were read before the reset was seen.
3. A reset that repeats: look at the supply (VSYS undervoltage), the watchdog service interval and `ReadPowerState()`.
4. A cable removal alone does not cause it: it puts `VBUS_LIN_ILIM` back to its OTP value (Table 163), which the driver
   does not take as a reset. The input limit is then the OTP one until the next `ServiceChargerWatchdog()`, which writes
   the configured limit again (or switches the charger off with `OutOfLimits` when it cannot).

## Configure returns VerifyFailed

A register read back differently from what was just written.

1. Another master on the bus (or the OTP reload after a reset) changed it: look for a second writer.
2. The chip reset behind the driver's back (a brown-out, RESETBMCU): the next call that changes the chip returns
   `DeviceReset` (see below); call `EnsureInitialized()` and `Configure()` again.
3. A read that is not stable (a noisy or floating line): the driver repeats a read until two agree, and reports
   `VerifyFailed` when a register keeps changing or reads all ones.
4. A field the datasheet marks as loaded from OTP may behave differently from the model (ambiguity C4): note the
   register and the value in the bring-up log.

## Configure returns ChargerFault, or SetChargerMode refuses

The charger reports a fault state; the parameters are written but charging is not enabled.

1. `ReadChargerStatus()` and read `fault`: `ThermistorSuspend` (battery outside the temperature window or no
   thermistor), `TimerFault` (a charge timer expired), `BatteryOvervoltage`, `BatteryMissing`, `ThermalShutdown`,
   `WatchdogExpired`.
2. Remove the cause, then `RestartCharger()`. After three attempts in a row without a charging state it returns
   `Refused`: look at the battery and the thermistor, not at the software.
3. With `watchdog.enable`, a `WatchdogExpired` means `ServiceChargerWatchdog()` was not called within the watchdog
   time: call it every 16 s or less.
4. `OutOfLimits` from `ReadChargerStatus()` or `ServiceInterrupts()` with the charger on: the input limit went back to
   an OTP value above `Limits` (a cable removal) and the charger block is not applied, so the charger was switched off.
   Apply the charger block, so that the configured limit is written again, and start charging.

## SetRailVoltage returns ReadOnlyRail

SW3's voltage is read only, and the voltage registers of SW1 and SW2 are written only when you declared
`BuckConfig::dvs = Dvs::Enabled`. The chip cannot report the OTP DVS mode (ambiguity C1): read your option's start-up
table (Table 85, `ReadVariant()`) and declare what it says.

## SetRailVoltage returns Timeout

The DVS-done flag did not rise within 256 us.

1. Is the rail enabled and loaded within its current limit? A rail that is off does not step.
2. Is `DelayUs()` waiting at least the requested time? A port whose delay returns early makes the wait end early.

## SetRailVoltage or SetRailModes returns OutOfLimits

The rail has no window in `Limits::rail[...]`, or the voltage is outside it. A rail the application did not describe
is never enabled or set. Add `{min_mv, max_mv, max_load_ma}` from the load's data sheet.

## EnterShip or EnterCoreOff returns Refused

A precondition is not met; nothing was written. `EnterShip()` needs state Run, Standby or Sleep, VBUS not valid and
below UVLO. `EnterCoreOff()` needs VBUS not valid and ONKEY not pushed. Unplug the supply (or confirm the state with
`ReadPowerState()`) and call again. The chip enters SHIP only with VBUSIN below 1.0 V (ambiguity C6): with the input
between 1 V and UVLO it ends in REGS_DISABLE.

## ServiceInterrupts returns nothing, though INTB was low

1. The source is masked (the default): `IrqMask{}` masks all. Unmask it with `SetIrqMask()` or
   `Config::irq_mask`.
2. With `GpioGet()`, the driver skips the bus while INTB is high. Does your `GpioGet()` report `asserted` when the pin
   is low?
3. Your own code cleared it: `ServiceInterrupts()` clears what it reports; call it from one place.

## Unexpected state after a reset

`ReadPowerState()` returns `Wait`, `Run`, `Standby`, `Sleep` or `RegsDisable`. A chip that reset behind the driver's
back (a brown-out, a long ONKEY press, the watchdog) re-reads its OTP values: call `EnsureInitialized()` (after it has
failed or on a new object) and `Configure()` again, and compare `SelfCheck()`.

## The charger reads a state the datasheet does not list

`ChargerState::Unknown` and `Fault::UnknownState` mean CHG_SNS or BATT_SNS held a reserved code. Read it again: the
first read after a reset may show the reset value (ambiguity C21).
