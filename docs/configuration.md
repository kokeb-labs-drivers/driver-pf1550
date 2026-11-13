# Configuration

Every setting the driver writes lives in one `pf1550::Config`. The constructor stores it, `EnsureInitialized()` checks
it (`Validate()`), and `Configure()` writes the blocks you marked `apply`.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From NXP Semiconductors PF1550 7:

    - Electrical limits and supply ranges: 5.3 Electrical characteristics, p. 12
    - SW1 and SW2: DVS and non-DVS operation: 6.2.2 SWx DVS and non-DVS operation, p. 26
    - SWx output voltage setting and DVS-done: 6.2.5 Output voltage setting in SWx, p. 28
    - SW3: 6.3 SW3 detailed description, p. 31
    - LDO1 and LDO3: 7.2 LDO1 and LDO3 detailed description, p. 35
    - LDO low-power mode: 7.2.5 LDOy low-power mode operation, p. 37
    - LDO2: 7.3 LDO2 detailed description, p. 38
    - VREFDDR: 7.4 VREFDDR reference, p. 40
    - VSNVS and the coin cell charger: 7.5 VSNVS LDO/switch, p. 41
    - Charger operating modes: 8.1 Operating modes and behavioral description, p. 43
    - Charging profile: 8.5 Charging profile, p. 46
    - Power path: VSYS regulation and input current limit: 8.7 Power path features, p. 55
    - Thermal regulation and foldback: 8.8 Thermal, p. 57
    - Input voltage regulation (DPM): 8.8.3 Input voltage regulation mode, p. 59
    - JEITA thermal control: 8.8.4 JEITA thermal control, p. 62
    - Battery overcurrent limit and BATFET: 8.9.6 Battery overcurrent limit, p. 66
    - Charger LED: 8.10 LED indicator, p. 67
    - Register OTP bitmap: 12.5 Register OTP bitmap, p. 128

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Limits

`Limits` is the application's own description of what its cell, supply and loads tolerate. It has **no defaults**: an
unset limit is rejected (`InvalidArgument`) before anything is sent, and a value outside a limit is `OutOfLimits`.
The numbers come from the data sheets of your cell, supply and processor, not from the PF1550's.

| Field | Range | Meaning |
| --- | --- | --- |
| `cell_max_mv` | 3500 to 4440 | Highest charge voltage the cell allows |
| `cell_max_charge_ma` | 100 to 1000 | Highest charge current the cell allows |
| `input_max_ma` | 10 to 1500 | What the supply on VBUS can deliver |
| `coin_cell_max_mv` | 0 or up to 3300 | Highest voltage the coin cell tolerates; 0 when there is no coin cell. A charge voltage above 1.8 V needs 3300 or more (ambiguity C40) |
| `rail[Rail]` | `{min_mv, max_mv, max_load_ma}` | The window the rail's load tolerates. All zero means "not set": the rail is never enabled or set (`OutOfLimits`) |

`SingleCellLimits()` returns 4.2 V, 500 mA and 1 A for a typical single cell: a starting point whose numbers you must
check, with no rail windows.

## Acknowledgements

A safety downgrade is accepted only with its flag in `Config::ack`. Every flag defaults to false, so the downgrade is
visible in your source and in review. A downgrade without its flag is `Unsafe`, with nothing sent.

| Flag | Needed for |
| --- | --- |
| `allow_untimed_charge` | `FastChargeTimer::Disabled` |
| `battery_has_own_ntc_protection` | `Thermistor::Ignore` |
| `allow_batfet_closed_on_fault` | Any `FaultPolicy::keep_closed_on_*` |
| `allow_batfet_oc_off` | `BatfetOc::Disabled`, `oc_keeps_closed` or `oc_without_vbus = false` (BOVRC_NOVBUS = 0 disables the protection without VBUS, Table 161); also the watchdog service, when BATFET_CNFG reads with the protection off |
| `eoc_accuracy_accepted` | Termination current 5 mA or 10 mA (wide accuracy band) |
| `load_below_10ma` | `RailModes::low_power` (specified for loads up to 10 mA) |
| `coin_cell_is_rechargeable` | `CoinCell::charge_enable` |
| `hard_reset_paths` | `PowerConfig::write_reset_paths` (ONKEY, PWRON, restart and global reset paths) |
| `undervoltage_threshold` | `PowerConfig::write_uvdet` |
| `allow_batfet_off` | `BatfetConfig::disable` (DISBATFET: the battery path is forced off) |
| `power_down_order_reviewed` | `PowerDownConfig::apply` and `SetPowerDownSequence()` |
| `dvs_on_unlisted_part` | `Dvs::Enabled` on a part that Table 85 does not list (A0, A4 to A9, unknown flavour), where the driver cannot cross-check the declaration (C57) |

## Blocks

A block with `apply = false` is left as the chip holds it. Fields with a unit carry it in the name. The defaults are
the chip's reset values, except where the table says the driver chose another.

### Charger (`ChargerConfig`)

| Field | Default | Range | Register |
| --- | --- | --- | --- |
| `mode` | `LinearOnly` | `Off`, `LinearOnly`, `Charge` | CHG_OPER, written last |
| `charge_voltage_mv` | none (set it) | 3500 to 4440 in 20 mV steps, at most `cell_max_mv` | CHGCV |
| `charge_current_ma` | none (set it) | 100 to 1000 in 50 mA steps, at most `cell_max_charge_ma` | CHG_CC |
| `termination_ma` | 50 | 5, 10, 20, 30, 50 | IEOC |
| `input_limit_ma` | none (set it) | 21 documented values, at most `input_max_ma` | VBUS_LIN_ILIM |
| `vsys_min` | 4.3 V | `SystemMin` | VSYSMIN (the reset value is 3.5 V) |
| `restart` | 150 mV | `Restart` | CHG_RESTART |
| `fast_timer` | 4 h | `FastChargeTimer` | FCHGTIME (`Disabled` needs an acknowledgement) |
| `eoc_timer`, `precharge_timer` | 10 min, 30 min | `EocTimer`, `PrechargeTimer` | EOCTIME, TPRECHG |
| `thermistor` | `Suspend` | `Suspend`, `Ignore`, ... | THM_CNFG |
| `thermal_reg`, `thermal_foldback` | 95 C, on | `ThermalRegTemp` | REGTEMP, TEMP_FB_EN (the reset value is off) |
| `watchdog` | off, 80 s | `WatchdogConfig{enable, time}` | WDTEN, WD_TIME; service within 16 s |
| `batfet` | reset values, `oc_without_vbus` true | `BatfetConfig` | BATFET_OC, BOVRC_DISBATFET, BOVRC_NOVBUS (true keeps the overcurrent protection on without VBUS; the reset value is off), DISBATFET, FAULT_BATFET_CNFG |
| `vbus` | reset values | `VbusOptions` | DPM, DPM stop, debounces, wake-up delay, VBUS2SYS |
| `jeita` | reset values | `JeitaConfig` | THM_COLD, THM_HOT, THM_COOL, THM_WARM, CV_ADJ, CC_ADJ |
| `interrupts` | off | `ChargerInterrupts` | CHG_INT_CNFG |

A value off the grid is `InvalidArgument`; one outside `Limits` is `OutOfLimits`. Codes the datasheet marks reserved
are not encodable. The charge voltage grid has no code for 3.50 V alone: codes 0 to 8 all mean 3.50 V and 56 to 63
all mean 4.44 V, so the driver encodes codes 8 to 55 (ambiguity C53).

### Bucks (`BuckConfig`: `sw1`, `sw2`, `sw3`)

| Field | Default | Meaning |
| --- | --- | --- |
| `dvs` | `Disabled` | **You declare** the OTP DVS mode (`OTP_SWx_DVS_SEL`); the chip cannot report it (C1). The voltage fields need `Enabled` |
| `run_uv`, `standby_uv`, `sleep_uv` | 0 keeps | The three DVS voltages, on the datasheet grid and inside the rail's window; SW1 and SW2 only |
| `modes` | all off | `RailModes{run, standby, sleep, low_power}`: EN, STBY_EN, OMODE, LPWR |
| `current_limit` | 1.0 A | `SwCurrentLimit` |
| `dvs_speed` | 2 us | `DvsSpeed`, SW1 and SW2 |
| `fpwm`, `fpwm_in_dvs` | off | Forced PWM |
| `output_discharge` | true | Discharge resistor when the rail is disabled |

### LDOs, VREFDDR, VSNVS, coin cell, USBPHY, LED

| Block | Fields |
| --- | --- |
| `ldo1`, `ldo2`, `ldo3` | `volt` (0 keeps), `modes`; LDO1 and LDO3 can become load switches only through `EnableLoadSwitch()` |
| `vrefddr` | `modes` |
| `vsnvs` | `local_bandgap_disable` (LIBGDIS) |
| `coin` | `charge_enable` (needs `coin_cell_is_rechargeable`), `voltage_mv` 1800 to 3300 in 100 mV steps (above 1800 needs `Limits::coin_cell_max_mv` of at least 3300; residual risk: Table 111 prints code 7 as 3.3 V with 100 mV steps, which does not agree, so the codes above 0 are not proven safe for a 3.6 V cell, C40) |
| `usbphy` | `enable`, `voltage`, `active_discharge` |
| `led` | `enable`, `duty_32nds` 0 to 32, `ramp`, `frequency`, `mode`, `software_override` |

### Power control (`PowerConfig`, `PowerDownConfig`)

| Field | Meaning |
| --- | --- |
| `write_standby`, `standby_delay`, `standby_inverted` | STANDBYDLY and STANDBYINV, written only with `write_standby`: STANDBYINV flips Run and Standby with the pin at a steady level, so change it only together with your port's `GpioSet()` |
| `write_debounce`, `pwron_debounce`, `onkey_debounce` | PWRONDBNC, ONKEYDBNC (codes 00 and 01 are identical, C41), written only with `write_debounce` |
| `write_regscp`, `ldo_off_on_current_limit` | REGSCPEN, written only with `write_regscp` |
| `write_low_sys_warn`, `low_sys_warn` | LOW_SYS_WARN threshold, written only with `write_low_sys_warn` |
| `write_reset_paths` and the four fields after it | PWRONRSTEN, RESTARTEN, ONKEY_RST_EN, TGRESET: need `hard_reset_paths` |
| `write_uvdet`, `uvdet` | UVDET: needs `undervoltage_threshold`; a wrong value resets the system |
| `power_down.sequence` | Order 0 to 7 per rail; needs `power_down_order_reviewed` |

### Interrupts and the rest

| Field | Default | Meaning |
| --- | --- | --- |
| `apply_irq_mask`, `irq_mask` | off, all masked | Writes the masks; 1 masks a source |
| `rail_order` | SW1, SW2, SW3, LDO1, LDO2, LDO3, VREFDDR | The order in which `Configure()` writes the rails: put a buck before the LDO it feeds |
| `as_found_policy` | `Report` | What `EnsureInitialized()` does with an unsafe charger found on: `Report` returns the error and writes nothing, `MakeSafe` switches it to linear-only |

## Example

```cpp title="A board"
--8<-- "tests/docs/configuration.cpp:custom"
```

## Validation

`pf1550::Validate(config)` checks a configuration without touching the chip, and `EnsureInitialized()` calls it before
any I/O. It is `constexpr`, so a fixed board configuration can be checked at compile time:

```cpp title="Check at compile time"
--8<-- "tests/docs/configuration.cpp:validate"
```

It returns `InvalidArgument` for unset limits, a value off the grid, a reserved code, a value that is not an
enumerator or a rail order that is not a permutation; `OutOfLimits` for a valid value outside `Limits`; `ReadOnlyRail`
for a buck voltage on SW3 or on a buck not declared `Dvs::Enabled`; `InvalidArgument` also for standby or sleep
enabled without run (EN gates both); `Unsafe` for a downgrade without its acknowledgement. The first failure is
returned, in this order: limits, rail order, charger, rails, peripherals, power.

## Kconfig

Under ESP-IDF, `idf.py menuconfig` shows **PF1550 power management IC** when the driver is a component of the project
([Installation](installation.md)); the menu is [`_config/Kconfig`](../_config/Kconfig). The options are only the
limits and the as-found policy, and they reach the code only through
[`inc/pf1550_kconfig.hpp`](../inc/pf1550_kconfig.hpp), never through a driver signature:

| Option | Default | Range | Sets |
| --- | --- | --- | --- |
| `PF1550_CELL_MAX_MV` | 4200 | 3500 to 4440 | `Limits::cell_max_mv` |
| `PF1550_CELL_MAX_CHARGE_MA` | 500 | 100 to 1000 | `Limits::cell_max_charge_ma` |
| `PF1550_INPUT_MAX_MA` | 1000 | 10 to 1500 | `Limits::input_max_ma` |
| `PF1550_AS_FOUND_MAKE_SAFE` | n | | `AsFoundPolicy::MakeSafe` |

```cpp title="Limits from menuconfig"
--8<-- "tests/docs/configuration.cpp:kconfig"
```

The defaults are a starting point for a 4.2 V cell: set them from your cell's data sheet before the first run.
