# Datasheet map

The driver follows the PF1550 datasheet. This page says which revision, where to get it, and where each part of
the driver is described in it, so you can check any behaviour against the source. Section names and page numbers
are the datasheet's own (the printed page numbers).


<!-- generated: datasheet-map -->
- **Datasheet: PF1550 Power management integrated circuit (PMIC) for low power application processors**, NXP
  Semiconductors PF1550, revision 7 (2021-09-29). Get it from the [vendor
  page](https://www.nxp.com/docs/en/data-sheet/PF1550.pdf).
  SHA-256 of the file used: `56e58b835c0e9e6e6b67a3211b4d82b3374b9d6d55ed7600750efc36e447bfa8`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Pins and package | 4.2 Pin definitions | p. 8 | `CtrlPin`, `I2cInterface` | [Hardware](hardware.md), [Quickstart](quickstart.md) |
| Electrical limits and supply ranges | 5.3 Electrical characteristics | p. 12 | `Limits`, `SingleCellLimits()`, `Validate()` | [Configuration](configuration.md), [Contract](contract.md) |
| SW1 and SW2: DVS and non-DVS operation | 6.2.2 SWx DVS and non-DVS operation | p. 26 | `BuckConfig::dvs`, `Dvs`, `ReadRail()` | [Configuration](configuration.md), [Contract](contract.md) |
| SWx output voltage setting and DVS-done | 6.2.5 Output voltage setting in SWx | p. 28 | `SetRailVoltage()`, `kDvsWaitMaxUs` | [API guide](api-guide.md), [Configuration](configuration.md) |
| SW3 | 6.3 SW3 detailed description | p. 31 | `Rail::Sw3`, `Error::ReadOnlyRail` | [Configuration](configuration.md) |
| LDO1 and LDO3 | 7.2 LDO1 and LDO3 detailed description | p. 35 | `LdoConfig`, `SetRailVoltage()`, `SetRailModes()` | [Configuration](configuration.md), [API guide](api-guide.md) |
| LDO low-power mode | 7.2.5 LDOy low-power mode operation | p. 37 | `RailModes::low_power`, `Acknowledgements::load_below_10ma` | [Configuration](configuration.md) |
| LDO load switch mode | 7.2.7 LDOy load switch mode | p. 38 | `EnableLoadSwitch()`, `LoadSwitchConfirm` | [API guide](api-guide.md), [Contract](contract.md) |
| LDO2 | 7.3 LDO2 detailed description | p. 38 | `Rail::Ldo2` | [Configuration](configuration.md) |
| VREFDDR | 7.4 VREFDDR reference | p. 40 | `VrefDdrConfig`, `Rail::VrefDdr` | [Configuration](configuration.md) |
| VSNVS and the coin cell charger | 7.5 VSNVS LDO/switch | p. 41 | `VsnvsConfig`, `CoinCell` | [Configuration](configuration.md) |
| Charger operating modes | 8.1 Operating modes and behavioral description | p. 43 | `SetChargerMode()`, `ChargerMode` | [API guide](api-guide.md), [Configuration](configuration.md) |
| Charger input source detection | 8.2 Charger input source detection | p. 45 | `ChargerStatus::vbus`, `VbusFlags` | [API guide](api-guide.md) |
| Charger state diagram | 8.4 Charger state diagram | p. 46 | `ChargerState`, `ReadChargerStatus()` | [API guide](api-guide.md) |
| Charging profile | 8.5 Charging profile | p. 46 | `ChargerConfig::charge_voltage_mv`, `charge_current_ma`, `termination_ma`, timers | [Configuration](configuration.md) |
| Power path: VSYS regulation and input current limit | 8.7 Power path features | p. 55 | `ChargerConfig::vsys_min`, `input_limit_ma`, `thermistor` | [Configuration](configuration.md) |
| Thermal regulation and foldback | 8.8 Thermal | p. 57 | `ChargerConfig::thermal_reg`, `thermal_foldback` | [Configuration](configuration.md) |
| Input voltage regulation (DPM) | 8.8.3 Input voltage regulation mode | p. 59 | `VbusOptions` | [Configuration](configuration.md) |
| JEITA thermal control | 8.8.4 JEITA thermal control | p. 62 | `JeitaConfig` | [Configuration](configuration.md) |
| Charger faults and restart | 8.9 Fault states | p. 64 | `Fault`, `RestartCharger()`, `kMaxChargerRestarts` | [API guide](api-guide.md), [Troubleshooting](troubleshooting.md) |
| Charger watchdog | 8.9.2 Watchdog timer state | p. 65 | `WatchdogConfig`, `ServiceChargerWatchdog()`, `kWatchdogServiceMaxS` | [API guide](api-guide.md), [Troubleshooting](troubleshooting.md) |
| Battery overcurrent limit and BATFET | 8.9.6 Battery overcurrent limit | p. 66 | `BatfetConfig`, `FaultPolicy` | [Configuration](configuration.md), [Contract](contract.md) |
| Charger LED | 8.10 LED indicator | p. 67 | `LedConfig` | [Configuration](configuration.md) |
| PWRON | 9.1 PWRON | p. 68 | `PowerConfig::pwron_debounce`, `ReadPowerState()` | [API guide](api-guide.md) |
| STANDBY pin | 9.2 STANDBY | p. 69 | `RequestStandby()`, `CtrlPin::Standby`, `PowerConfig::standby_inverted` | [API guide](api-guide.md), [Porting](porting.md) |
| RESETBMCU | 9.3 RESETBMCU | p. 69 | `PowerConfig`, `StartupProfileOf()` | [API guide](api-guide.md) |
| INTB and the interrupt registers | 9.4 INTB | p. 70 | `ServiceInterrupts()`, `SetIrqMask()`, `CtrlPin::Intb`, `IrqSource` | [API guide](api-guide.md), [Porting](porting.md) |
| I2C address, clock and register framing | 9.7 Control interface I2C block description | p. 71 | `I2cInterface`, `I2cPort`, `kI2cAddress`, `kMaxClockHz`, every register access | [Hardware](hardware.md), [Porting](porting.md), [Architecture](architecture.md) |
| ONKEY | 9.6 ONKEY | p. 71 | `PowerConfig::onkey_debounce`, `IrqSource::OnkeyPush` | [API guide](api-guide.md) |
| I2C operation: single-register transfers | 9.7.2 I2C operation | p. 72 | `Write()`, `WriteRead()`, `kRepeatedStart` | [Porting](porting.md), [Architecture](architecture.md) |
| State machine and transitions | 10 PF1550 state machine | p. 72 | `ReadPowerState()`, `PowerState` | [API guide](api-guide.md), [Architecture](architecture.md) |
| System ON, OFF, SHIP and CORE_OFF | 10.2 System OFF states | p. 74 | `EnterShip()`, `EnterCoreOff()`, `ShipConfirm`, `CoreOffConfirm` | [API guide](api-guide.md), [Contract](contract.md) |
| State transition conditions (Table 83) | 10.5 State diagram and transition conditions | p. 76 | `EnterShip()`, `EnterCoreOff()`, `Error::Refused` | [Contract](contract.md) |
| Regulator power-up sequencer | 10.6 Regulator power-up sequencer | p. 77 | `StartupProfileOf()`, `PowerUpDelayUs()`, `PowerUpDurationUs()` | [API guide](api-guide.md) |
| Regulator power-down sequencer | 10.7 Regulator power-down sequencer | p. 78 | `ReadPowerDownSequence()`, `SetPowerDownSequence()`, `PowerDownDelayUs()` | [API guide](api-guide.md), [Contract](contract.md) |
| Device start-up timing and pre-programmed options | 11 Device start up | p. 78 | `ColdStartToResetReleaseUs()`, `ResetReleaseDelayMs()`, `Option` | [API guide](api-guide.md) |
| Register map and access types | 12 Register map | p. 80 | `inc/pf1550_registers.hpp`, `IsWritable()` | [Architecture](architecture.md), [Contract](contract.md) |
| Device ID, silicon revision and OTP flavour | 12.1 Specific PMIC Registers (Offset is 0x00) | p. 80 | `EnsureInitialized()`, `ReadVariant()`, `Error::DeviceIdMismatch` | [API guide](api-guide.md), [Troubleshooting](troubleshooting.md) |
| Register OTP bitmap | 12.5 Register OTP bitmap | p. 128 | `Dvs`, `BuckConfig::dvs`; the OTP image is not read back | [Configuration](configuration.md), [Contract](contract.md) |
| Example schematic | 13.1 Example schematic | p. 133 | - | [Hardware](hardware.md) |
| Package outline | 14 Package outline | p. 139 | - | [Hardware](hardware.md) |
<!-- /generated -->

## What is not covered

The datasheet is covered feature by feature: the [coverage matrix](coverage-matrix.md) has one row per fact, and
[Coverage](coverage.md) counts every register, bit field, interrupt
source, state, pin, electrical parameter, timing and OTP value, and says which are implemented, modelled and tested.
One item the datasheet cannot support is left out, and one family of values is reported only by option:

| Item | Why | How to close it |
| --- | --- | --- |
| OTP image read-back (`OF-RB`) | Rev. 7 documents no read window for the fuses (the KEY1 register at `0x6F` is listed, no key sequence or window is: ambiguity C5). The driver therefore cannot read the OTP values over I2C | Obtain the NXP document that describes the OTP read procedure; add a typed read of the image, with a model and a test |
| OTP start-up values of A0 and of unlisted flavours | A0 is "not programmed, internal validation only" (Table 1); flavour values above 9 are not listed (ambiguity C10) | Read the registers the chip holds: the driver reports raw bytes and `Option::Unknown`, and assumes nothing |
| DVS mode of SW1 and SW2 (`OTP_SWx_DVS_SEL`) | A fuse with no register (ambiguity C1) | You declare it in `BuckConfig::dvs`; the model's option table gives its own reading of Table 85 |
| Analog values (accuracy bands, quiescent currents, rise times, thresholds the driver never programs) | Properties of the chip, not of the driver: the matrix records them as documented decisions | None; they are listed in the coverage matrix as information rows |

## What to verify on the bench

Nothing in this driver has been verified on hardware. The behaviours below rest on the datasheet text alone and each
has a model knob ([Ambiguities](ambiguities.md)); a bench measurement settles them.

| Behaviour | Bench step | Rows |
| --- | --- | --- |
| Interrupt clearing: write-1-to-clear or clear-on-read | Set a status bit (plug a supply), read the status register twice, then write the bit back | C3, C14 |
| `RW1S` fields: does a written 0 clear a bit loaded from OTP | Write 0 to a charger field that start-up set, read it back | C4 |
| Watchdog expiry code and period | Enable WDTEN, stop servicing, time the expiry and read CHG_SNS | C8, C9 |
| Charger parameter changes while charging | Change the current in mode `Charge` and watch the charge current | C16, C49 |
| DVS-done timing | Step SW1 or SW2 and measure the time to DVS-done at each `DVSSPEED` | C34 |
| SHIP with VBUS between 1 V and UVLO | Call `EnterShip()` with the input at 2 V | C6 |
| CORE_OFF outside a system-on state | Call `EnterCoreOff()` in Standby | C7 |
| Coin cell charge voltage code | Program `VCOIN` and measure LICELL | C40 |
| Power-up slot numbering, equal and zero power-down orders | Measure the rails at power-up and power-down with a logic analyser | C54, C55, C56 |
| Charge-voltage and current accuracy | Measure VBATT in constant-voltage charging at the configured value | Table 5 |

## When the datasheet and the driver differ

The driver is written against the revision above. If you have a newer revision, compare its revision history with
the topics above; a change to a pin, a register, a timing or a value the driver uses is a bug report for this
repository. Where the driver deliberately goes beyond the datasheet, the [Contract](contract.md) says so.

The repository never contains the PDF itself: vendors own their documents. Download it from the vendor, and compare
its SHA-256 with the one above to confirm you have the same file.
