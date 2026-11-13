/**
 * @file pf1550_config.hpp
 * @brief PF1550 configuration: the application's `Limits`, its explicit acknowledgements, the typed
 * configuration of every block and `Validate()`.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The chip is a power supply: a wrong byte can overcharge a cell or overvolt a processor. The
 * driver therefore has no public raw register write. Everything is written through the typed values
 * below, after `Validate()` has checked them against the data sheet grids and against `Limits`, the
 * application's own description of what its battery, supply and loads tolerate. `Limits` has no
 * defaults: an unset limit is rejected. A safety downgrade (no charge timer, thermistor ignored, no
 * battery overcurrent protection, a hard-reset path changed) is accepted only with the matching
 * flag of `Acknowledgements`, so that it is visible in the application's source.
 */
#ifndef PF1550_CONFIG_HPP
#define PF1550_CONFIG_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_codec.hpp"
#include "pf1550_detail.hpp"
#include "pf1550_sequencer.hpp"
#include "pf1550_types.hpp"

namespace pf1550 {

/**
 * @defgroup pf1550_config Configuration
 * @brief Limits, acknowledgements, block configurations and validation.
 * @{
 */

/** @brief The voltage window and load a rail's consumer tolerates. All zero means "not set". */
struct RailLimits {
  uint16_t min_mv{0}; ///< Lowest voltage the load tolerates, mV.
  uint16_t max_mv{0}; ///< Highest voltage the load tolerates, mV; 0 forbids configuring the rail.
  uint16_t max_load_ma{0}; ///< The load's current, mA; 0 when not declared (informational).
};

/** @brief What the application's hardware tolerates. No defaults: `Validate()` rejects unset ones.
 */
struct Limits {
  uint16_t cell_max_mv{0};        ///< Highest charge voltage the cell allows, 3500 to 4440 mV.
  uint16_t cell_max_charge_ma{0}; ///< Highest charge current the cell allows, 100 to 1000 mA.
  uint16_t input_max_ma{0};       ///< What the supply can deliver, 10 to 1500 mA.
  uint16_t coin_cell_max_mv{
      0}; ///< Highest voltage the coin cell tolerates (needs >= 3300 for a
          ///< charge voltage above 1.8 V, ambiguity C40); 0 when no coin cell.
  std::array<RailLimits, kRailCount> rail{}; ///< Per rail, indexed by `Rail`.
};

/** @brief Explicit acknowledgements; every flag defaults to false. */
struct Acknowledgements {
  bool allow_untimed_charge{false};           ///< Charge with the fast-charge timer disabled.
  bool battery_has_own_ntc_protection{false}; ///< Thermistor ignored by the charger.
  bool allow_batfet_closed_on_fault{false};   ///< BATFET stays closed on a charger fault.
  bool allow_batfet_oc_off{false};            ///< Battery overcurrent protection reduced or off.
  bool eoc_accuracy_accepted{false};          ///< 5 or 10 mA termination (wide accuracy band).
  bool load_below_10ma{false};                ///< Low-power mode on a rail (specified to 10 mA).
  bool coin_cell_is_rechargeable{false};      ///< Coin cell charger enabled.
  bool hard_reset_paths{false};               ///< Changing ONKEY, PWRON, restart or TGRESET paths.
  bool undervoltage_threshold{false};         ///< Changing the VSYS undervoltage detect threshold.
  bool allow_batfet_off{false};               ///< DISBATFET: battery path forced off.
  bool power_down_order_reviewed{false};      ///< Changing the power-down sequence.
  bool dvs_on_unlisted_part{false};           ///< DVS declared on a part Table 85 does not list.
};

/** @brief Per-state rail enables (EN, STBY_EN, OMODE, LPWR). */
struct RailModes {
  bool run{false};       ///< Enabled in RUN.
  bool standby{false};   ///< Enabled in STANDBY.
  bool sleep{false};     ///< Enabled in SLEEP.
  bool low_power{false}; ///< Low-power mode in STANDBY and SLEEP; needs `load_below_10ma`.
};

/** @brief A buck regulator. `apply = false` leaves the block as the chip holds it. */
struct BuckConfig {
  bool apply{false};       ///< Write this block in `Configure()`.
  Dvs dvs{Dvs::Disabled};  ///< OTP_SWx_DVS_SEL, which the chip cannot report.
  Microvolts run_uv{};     ///< SWx_VOLT (DVS only); 0 keeps it.
  Microvolts standby_uv{}; ///< SWx_STBY_VOLT (DVS only); 0 keeps it.
  Microvolts sleep_uv{};   ///< SWx_SLP_VOLT (DVS only); 0 keeps it.
  RailModes modes{};       ///< Enables.
  SwCurrentLimit current_limit{SwCurrentLimit::A1p0}; ///< SWx_ILIM.
  DvsSpeed dvs_speed{DvsSpeed::Us2};                  ///< SWx_DVSSPEED (SW1 and SW2).
  bool fpwm{false};                                   ///< Forced PWM.
  bool fpwm_in_dvs{false};                            ///< Forced PWM while the voltage steps down.
  bool output_discharge{true}; ///< Discharge resistor when disabled (RDIS_ENB).
};

/** @brief An LDO. */
struct LdoConfig {
  bool apply{false}; ///< Write this block in `Configure()`.
  Microvolts volt{}; ///< LDOx_VOLT; 0 keeps it.
  RailModes modes{}; ///< Enables.
};

/** @brief VREFDDR. */
struct VrefDdrConfig {
  bool apply{false}; ///< Write this block in `Configure()`.
  RailModes modes{}; ///< Enables.
};

/** @brief VSNVS. */
struct VsnvsConfig {
  bool apply{false};                 ///< Write this block in `Configure()`.
  bool local_bandgap_disable{false}; ///< LIBGDIS: saves quiescent current in coin cell mode.
};

/** @brief The coin cell charger (Table 111). */
struct CoinCell {
  bool apply{false};         ///< Write this block in `Configure()`.
  bool charge_enable{false}; ///< COINCHEN; needs `coin_cell_is_rechargeable`.
  uint16_t voltage_mv{1800}; ///< VCOIN, 1800 to 3300 in 100 mV steps (needs `coin_cell_max_mv`).
};

/** @brief The USBPHY LDO (Table 165). */
struct UsbPhyConfig {
  bool apply{false};                          ///< Write this block in `Configure()`.
  bool enable{false};                         ///< USBPHYLDO.
  UsbPhyVoltage voltage{UsbPhyVoltage::V3p3}; ///< USBPHY.
  bool active_discharge{true};                ///< ACTDISPHY.
};

/** @brief The charger LED driver (Tables 170 and 172). */
struct LedConfig {
  bool apply{false};                         ///< Write this block in `Configure()`.
  bool enable{false};                        ///< LED_EN.
  uint8_t duty_32nds{0};                     ///< LED_PWM duty n/32, 0 to 32 (ambiguity C43).
  bool ramp{false};                          ///< LED_RAMP.
  LedFrequency frequency{LedFrequency::Hz1}; ///< LED_FREQ.
  LedMode mode{LedMode::OnWhileCharging};    ///< LED_CFG.
  bool software_override{false};             ///< LEDOVRD.
};

/** @brief Power control (Tables 135 to 137). */
struct PowerConfig {
  bool apply{false}; ///< Write the fields below whose own `write_...` flag is set in `Configure()`.
  bool write_standby{false}; ///< Write `standby_delay` and `standby_inverted`.
  StandbyDelay standby_delay{StandbyDelay::Cycles1}; ///< STANDBYDLY.
  bool standby_inverted{
      false};                 ///< STANDBYINV: 1 = the STANDBY pin is active low (flips the state).
  bool write_debounce{false}; ///< Write both debounce times.
  Debounce pwron_debounce{Debounce::Ms31};     ///< PWRONDBNC.
  Debounce onkey_debounce{Debounce::Ms31};     ///< ONKEYDBNC.
  bool write_regscp{false};                    ///< Write `ldo_off_on_current_limit`.
  bool ldo_off_on_current_limit{false};        ///< REGSCPEN.
  bool write_low_sys_warn{false};              ///< Write `low_sys_warn`.
  LowSysWarn low_sys_warn{LowSysWarn::Mv3300}; ///< LOW_SYS_WARN.
  bool write_reset_paths{false}; ///< Also write the next four fields; needs `hard_reset_paths`.
  bool pwron_long_press_reset{false};        ///< PWRONRSTEN.
  bool restart_on_long_press{false};         ///< RESTARTEN.
  bool onkey_reset{true};                    ///< ONKEY_RST_EN.
  GlobalReset global_reset{GlobalReset::S4}; ///< TGRESET.
  bool write_uvdet{false};    ///< Also write `uvdet`; needs `undervoltage_threshold`.
  Uvdet uvdet{Uvdet::Mv2650}; ///< UVDET.
};

/** @brief The power-down order (Tables 139 to 145); needs `power_down_order_reviewed`. */
struct PowerDownConfig {
  bool apply{false};            ///< Write the order in `Configure()`.
  PowerDownSequence sequence{}; ///< Order values 0 to 7 per rail.
};

/** @brief VBUS options (Tables 164, 166, 169). */
struct VbusOptions {
  DpmVoltage dpm{DpmVoltage::Mv3900};                            ///< VBUS_DPM_REG.
  DpmStop dpm_stop{DpmStop::Mv200};                              ///< VIN_DPM_STOP.
  ShortDebounce ovlo_debounce{ShortDebounce::Us100};             ///< VBUS_OV_TDB.
  ShortDebounce vbus2sys_debounce{ShortDebounce::Us100};         ///< VBUS2SYS_TDB.
  WakeupDelay sys_wakeup_delay{WakeupDelay::Ms8};                ///< SYS_WKUP_DLY.
  Vbus2SysThreshold vbus2sys_threshold{Vbus2SysThreshold::Mv50}; ///< VBUS2SYS_THRSH.
};

/** @brief JEITA thresholds and reductions (Tables 162, 168). */
struct JeitaConfig {
  Cold cold{Cold::C0};                 ///< THM_COLD.
  Hot hot{Hot::C60};                   ///< THM_HOT.
  Cool cool{Cool::C15};                ///< THM_COOL.
  Warm warm{Warm::C45};                ///< THM_WARM.
  CvAdjust cv_adjust{CvAdjust::Mv60};  ///< CV_ADJ.
  CcAdjust cc_adjust{CcAdjust::Pct25}; ///< CC_ADJ.
};

/** @brief BATFET behaviour per fault; false opens the BATFET (the reset value, the safe one). */
struct FaultPolicy {
  bool keep_closed_on_overvoltage{false};      ///< OVFLT_BFET_EN.
  bool keep_closed_on_watchdog{false};         ///< WDFLT_BFET_EN.
  bool keep_closed_on_thermistor{false};       ///< THMSUS_BFET_EN.
  bool keep_closed_on_thermal_shutdown{false}; ///< TSHDN_BFET_EN.
  bool keep_closed_on_timer_fault{false};      ///< TMRFLT_BFET_EN.
};

/** @brief The battery FET (Tables 156, 161, 171). */
struct BatfetConfig {
  bool disable{false};                   ///< DISBATFET; needs `allow_batfet_off`.
  BatfetOc oc_threshold{BatfetOc::A3p2}; ///< BATFET_OC; Disabled needs `allow_batfet_oc_off`.
  bool oc_keeps_closed{false};           ///< BOVRC_DISBATFET; needs `allow_batfet_oc_off`.
  bool oc_without_vbus{true}; ///< BOVRC_NOVBUS: true keeps the protection on without VBUS (Table
                              ///< 161); false needs `allow_batfet_oc_off`.
  FaultPolicy
      fault_policy{}; ///< FAULT_BATFET_CNFG; keep closed needs `allow_batfet_closed_on_fault`.
};

/** @brief The charger watchdog. When on, `ServiceChargerWatchdog()` must run every 16 s or less. */
struct WatchdogConfig {
  bool enable{false};                   ///< WDTEN.
  WatchdogTime time{WatchdogTime::S80}; ///< WD_TIME.
};

/** @brief Charger interrupt options (Table 167). */
struct ChargerInterrupts {
  bool every_transition{false}; ///< CHG_INT_GEN.
  bool eoc{false};              ///< EOC_INT.
};

/** @brief The charger. `apply = false` leaves it as found. Voltage, current and input limit have no
 * default: they must be set when `apply` is true. */
struct ChargerConfig {
  bool apply{false};                         ///< Write the charger in `Configure()`.
  ChargerMode mode{ChargerMode::LinearOnly}; ///< CHG_OPER, written last.
  uint16_t charge_voltage_mv{0}; ///< CHGCV, 3500 to 4440 step 20; at most `cell_max_mv`.
  uint16_t charge_current_ma{0}; ///< CHG_CC, 100 to 1000 step 50; at most `cell_max_charge_ma`.
  uint16_t termination_ma{50};   ///< IEOC: 5, 10, 20, 30 or 50.
  uint16_t input_limit_ma{0};    ///< VBUS_LIN_ILIM, one of 21 values; at most `input_max_ma`.
  SystemMin vsys_min{SystemMin::Mv4300}; ///< VSYSMIN (differs from the reset value 3.5 V).
  Restart restart{Restart::Mv150};       ///< CHG_RESTART.
  FastChargeTimer fast_timer{FastChargeTimer::Hours4};   ///< FCHGTIME.
  EocTimer eoc_timer{EocTimer::Min10};                   ///< EOCTIME.
  PrechargeTimer precharge_timer{PrechargeTimer::Min30}; ///< TPRECHG.
  Thermistor thermistor{Thermistor::Suspend};            ///< THM_CNFG.
  ThermalRegTemp thermal_reg{ThermalRegTemp::C95};       ///< REGTEMP.
  bool thermal_foldback{true};    ///< TEMP_FB_EN (differs from the reset value 0).
  WatchdogConfig watchdog{};      ///< WDTEN and WD_TIME.
  BatfetConfig batfet{};          ///< Battery FET.
  VbusOptions vbus{};             ///< VBUS options.
  JeitaConfig jeita{};            ///< JEITA.
  ChargerInterrupts interrupts{}; ///< CHG_INT_CNFG.
};

/** @brief Everything the driver writes. The constructor stores it with no I/O. */
struct Config {
  Limits limits{};              ///< Mandatory.
  Acknowledgements ack{};       ///< Explicit downgrades.
  ChargerConfig charger{};      ///< The charger.
  BuckConfig sw1{};             ///< Buck 1.
  BuckConfig sw2{};             ///< Buck 2.
  BuckConfig sw3{};             ///< Buck 3 (voltage read only).
  LdoConfig ldo1{};             ///< LDO1.
  LdoConfig ldo2{};             ///< LDO2.
  LdoConfig ldo3{};             ///< LDO3.
  VrefDdrConfig vrefddr{};      ///< VREFDDR.
  VsnvsConfig vsnvs{};          ///< VSNVS.
  CoinCell coin{};              ///< Coin cell charger.
  UsbPhyConfig usbphy{};        ///< USBPHY LDO.
  LedConfig led{};              ///< LED.
  PowerConfig power{};          ///< Power control.
  PowerDownConfig power_down{}; ///< Power-down order.
  bool apply_irq_mask{false};   ///< Write `irq_mask` in `Configure()`.
  IrqMask irq_mask{};           ///< 1 masks a source; default masks all.
  /// The order in which `Configure()` writes the rails: a buck that feeds an LDO goes first.
  std::array<Rail, kRailCount> rail_order{Rail::Sw1,  Rail::Sw2,  Rail::Sw3,    Rail::Ldo1,
                                          Rail::Ldo2, Rail::Ldo3, Rail::VrefDdr};
  AsFoundPolicy as_found_policy{
      AsFoundPolicy::Report}; ///< What to do with an unsafe charger found on.
};

/// Whether a value is a documented grid member and inside a window; helpers of `Validate()`.
namespace detail {

/// True when `value` lies in `[low, high]`.
constexpr bool InRange(uint32_t value, uint32_t low, uint32_t high) noexcept {
  return (value >= low) && (value <= high);
}

/// The limits of one rail.
constexpr const RailLimits& LimitsOf(const Limits& limits, Rail rail) noexcept {
  return limits.rail[static_cast<std::size_t>(ToUnderlying(rail)) % kRailCount];
}

/// Limits must be set and sane.
constexpr Error ValidateLimits(const Limits& l) noexcept {
  if (!InRange(l.cell_max_mv, kChargeVoltageMinMv, kChargeVoltageMaxMv) ||
      !InRange(l.cell_max_charge_ma, kChargeCurrentMinMa, kChargeCurrentMaxMa) ||
      !InRange(l.input_max_ma, 10_u32, 1500_u32)) {
    return Error::InvalidArgument;
  }
  for (const RailLimits& r : l.rail) {
    if (r.min_mv > r.max_mv) {
      return Error::InvalidArgument;
    }
  }
  return Error::Ok;
}

/// Grid and limit checks of the charger values.
constexpr Error ValidateChargerValues(const ChargerConfig& c, const Limits& l) noexcept {
  if (!ChargeVoltageCode(c.charge_voltage_mv).ok() ||
      !ChargeCurrentCode(c.charge_current_ma).ok() || !TerminationCode(c.termination_ma).ok() ||
      !InputLimitCode(c.input_limit_ma).ok()) {
    return Error::InvalidArgument;
  }
  if ((c.charge_voltage_mv > l.cell_max_mv) || (c.charge_current_ma > l.cell_max_charge_ma) ||
      (c.input_limit_ma > l.input_max_ma)) {
    return Error::OutOfLimits;
  }
  return Error::Ok;
}

/// True when every value is an enumerator of its type.
template <typename... E>
constexpr bool AllEnumerators(const E&... values) noexcept {
  return (IsEnumerator(values) && ...);
}

/// Enumerator checks of the charger.
constexpr Error ValidateChargerEnums(const ChargerConfig& c) noexcept {
  const bool ok =
      AllEnumerators(c.mode, c.vsys_min, c.restart, c.fast_timer, c.eoc_timer, c.precharge_timer,
                     c.thermistor, c.thermal_reg, c.watchdog.time, c.batfet.oc_threshold);
  const bool vbus_ok =
      AllEnumerators(c.vbus.dpm, c.vbus.dpm_stop, c.vbus.ovlo_debounce, c.vbus.vbus2sys_debounce,
                     c.vbus.sys_wakeup_delay, c.vbus.vbus2sys_threshold);
  const bool jeita_ok = AllEnumerators(c.jeita.cold, c.jeita.hot, c.jeita.cool, c.jeita.warm,
                                       c.jeita.cv_adjust, c.jeita.cc_adjust);
  return (ok && vbus_ok && jeita_ok) ? Error::Ok : Error::InvalidArgument;
}

/// A downgrade that is not acknowledged.
constexpr bool Unacknowledged(bool downgrade, bool acknowledged) noexcept {
  return downgrade && !acknowledged;
}

/// Whether the BATFET is configured to stay closed on any fault.
constexpr bool KeepsClosedOnFault(const FaultPolicy& p) noexcept {
  return p.keep_closed_on_overvoltage || p.keep_closed_on_watchdog || p.keep_closed_on_thermistor ||
         p.keep_closed_on_thermal_shutdown || p.keep_closed_on_timer_fault;
}

/// Acknowledgements for the timer, the thermistor and the termination current.
constexpr bool ChargerProtectionUnacknowledged(const ChargerConfig& c,
                                               const Acknowledgements& a) noexcept {
  return Unacknowledged(c.fast_timer == FastChargeTimer::Disabled, a.allow_untimed_charge) ||
         Unacknowledged(c.thermistor == Thermistor::Ignore, a.battery_has_own_ntc_protection) ||
         Unacknowledged(c.termination_ma < 20U, a.eoc_accuracy_accepted);
}

/// Acknowledgements for the battery FET.
constexpr bool BatfetUnacknowledged(const BatfetConfig& b, const Acknowledgements& a) noexcept {
  const bool oc_off =
      (b.oc_threshold == BatfetOc::Disabled) || b.oc_keeps_closed || !b.oc_without_vbus;
  return Unacknowledged(KeepsClosedOnFault(b.fault_policy), a.allow_batfet_closed_on_fault) ||
         Unacknowledged(oc_off, a.allow_batfet_oc_off) ||
         Unacknowledged(b.disable, a.allow_batfet_off);
}

/// Acknowledgements the charger needs for its safety downgrades.
constexpr Error ValidateChargerAcks(const ChargerConfig& c, const Acknowledgements& a) noexcept {
  const bool unsafe = ChargerProtectionUnacknowledged(c, a) || BatfetUnacknowledged(c.batfet, a);
  return unsafe ? Error::Unsafe : Error::Ok;
}

/// The charger block.
constexpr Error ValidateCharger(const Config& c) noexcept {
  if (!c.charger.apply) {
    return Error::Ok;
  }
  if (const Error e = ValidateChargerEnums(c.charger); e != Error::Ok) {
    return e;
  }
  if (const Error e = ValidateChargerValues(c.charger, c.limits); e != Error::Ok) {
    return e;
  }
  return ValidateChargerAcks(c.charger, c.ack);
}

/// A rail's enables against its limits and the low-power acknowledgement.
constexpr Error ValidateModes(const RailModes& m, const RailLimits& r,
                              const Acknowledgements& a) noexcept {
  const bool any = m.run || m.standby || m.sleep;
  if ((m.standby || m.sleep) && !m.run) {
    return Error::InvalidArgument; // Table 42: STBY_EN and OMODE act only with EN = 1
  }
  if (any && (r.max_mv == 0U)) {
    return Error::OutOfLimits;
  }
  if (m.low_power && !a.load_below_10ma) {
    return Error::Unsafe;
  }
  return Error::Ok;
}

/// A voltage the caller asks for: on the rail's grid and inside its limits; 0 keeps the voltage.
constexpr Error ValidateVolt(Rail rail, Microvolts v, const Limits& l) noexcept {
  if (v.value == 0U) {
    return Error::Ok;
  }
  const bool buck = (rail == Rail::Sw1) || (rail == Rail::Sw2);
  const bool on_grid = buck ? BuckDvsCode(v.value).ok() : LdoCode(rail, v.value).ok();
  if (!on_grid) {
    return Error::InvalidArgument;
  }
  const RailLimits& r = LimitsOf(l, rail);
  const uint32_t mv = v.value / 1000U;
  return ((r.max_mv != 0U) && (mv >= r.min_mv) && (mv <= r.max_mv)) ? Error::Ok
                                                                    : Error::OutOfLimits;
}

/// One buck block.
constexpr Error ValidateBuck(const BuckConfig& b, Rail rail, const Config& c) noexcept {
  if (!b.apply) {
    return Error::Ok;
  }
  if (!IsEnumerator(b.dvs) || !IsEnumerator(b.current_limit) || !IsEnumerator(b.dvs_speed)) {
    return Error::InvalidArgument;
  }
  const bool sets_volt =
      (b.run_uv.value != 0U) || (b.standby_uv.value != 0U) || (b.sleep_uv.value != 0U);
  if (sets_volt && ((rail == Rail::Sw3) || (b.dvs != Dvs::Enabled))) {
    return Error::ReadOnlyRail;
  }
  for (const Microvolts v : {b.run_uv, b.standby_uv, b.sleep_uv}) {
    if (const Error e = ValidateVolt(rail, v, c.limits); e != Error::Ok) {
      return e;
    }
  }
  return ValidateModes(b.modes, LimitsOf(c.limits, rail), c.ack);
}

/// One LDO block.
constexpr Error ValidateLdo(const LdoConfig& b, Rail rail, const Config& c) noexcept {
  if (!b.apply) {
    return Error::Ok;
  }
  if (const Error e = ValidateVolt(rail, b.volt, c.limits); e != Error::Ok) {
    return e;
  }
  return ValidateModes(b.modes, LimitsOf(c.limits, rail), c.ack);
}

/// The rails, coin cell, USBPHY and LED.
constexpr Error ValidateRails(const Config& c) noexcept {
  const std::array<Error, 3> buck = {{ValidateBuck(c.sw1, Rail::Sw1, c),
                                      ValidateBuck(c.sw2, Rail::Sw2, c),
                                      ValidateBuck(c.sw3, Rail::Sw3, c)}};
  const std::array<Error, 3> ldo = {{ValidateLdo(c.ldo1, Rail::Ldo1, c),
                                     ValidateLdo(c.ldo2, Rail::Ldo2, c),
                                     ValidateLdo(c.ldo3, Rail::Ldo3, c)}};
  for (const Error e : buck) {
    if (e != Error::Ok) {
      return e;
    }
  }
  for (const Error e : ldo) {
    if (e != Error::Ok) {
      return e;
    }
  }
  if (c.vrefddr.apply) {
    return ValidateModes(c.vrefddr.modes, LimitsOf(c.limits, Rail::VrefDdr), c.ack);
  }
  return Error::Ok;
}

/// Coin cell, USBPHY and LED.
constexpr Error ValidatePeripherals(const Config& c) noexcept {
  if (c.coin.apply) {
    if (!CoinVoltageCode(c.coin.voltage_mv).ok()) {
      return Error::InvalidArgument;
    }
    if (c.coin.charge_enable && !c.ack.coin_cell_is_rechargeable) {
      return Error::Unsafe;
    }
    if ((c.coin.voltage_mv > 1800U) && (c.limits.coin_cell_max_mv < 3300U)) {
      return Error::OutOfLimits;
    }
  }
  if (c.usbphy.apply && !IsEnumerator(c.usbphy.voltage)) {
    return Error::InvalidArgument;
  }
  if (c.led.apply &&
      ((c.led.duty_32nds > 32U) || !IsEnumerator(c.led.frequency) || !IsEnumerator(c.led.mode))) {
    return Error::InvalidArgument;
  }
  return Error::Ok;
}

/// Power control: enumerators and the acknowledgements of the guarded fields.
constexpr Error ValidatePowerControl(const PowerConfig& p, const Acknowledgements& a) noexcept {
  if (!p.apply) {
    return Error::Ok;
  }
  const bool enums = AllEnumerators(p.standby_delay, p.pwron_debounce, p.onkey_debounce,
                                    p.low_sys_warn, p.global_reset, p.uvdet);
  if (!enums) {
    return Error::InvalidArgument;
  }
  const bool unsafe = Unacknowledged(p.write_reset_paths, a.hard_reset_paths) ||
                      Unacknowledged(p.write_uvdet, a.undervoltage_threshold);
  return unsafe ? Error::Unsafe : Error::Ok;
}

/// The power-down order: values 0 to 7 and the acknowledgement.
constexpr Error ValidatePowerDown(const PowerDownConfig& d, const Acknowledgements& a) noexcept {
  if (!d.apply) {
    return Error::Ok;
  }
  for (const uint8_t v : d.sequence.order) {
    if (v > kMaxSequenceNumber) {
      return Error::InvalidArgument;
    }
  }
  return a.power_down_order_reviewed ? Error::Ok : Error::Unsafe;
}

/// Power control and the power-down order.
constexpr Error ValidatePower(const Config& c) noexcept {
  if (const Error e = ValidatePowerControl(c.power, c.ack); e != Error::Ok) {
    return e;
  }
  return ValidatePowerDown(c.power_down, c.ack);
}

/// The rail order is a permutation of the rails.
constexpr Error ValidateOrder(const Config& c) noexcept {
  std::array<bool, kRailCount> seen{};
  for (const Rail r : c.rail_order) {
    if (!IsEnumerator(r)) {
      return Error::InvalidArgument;
    }
    seen[static_cast<std::size_t>(ToUnderlying(r))] = true;
  }
  for (const bool s : seen) {
    if (!s) {
      return Error::InvalidArgument;
    }
  }
  return IsEnumerator(c.as_found_policy) ? Error::Ok : Error::InvalidArgument;
}

} // namespace detail

/**
 * @brief Check a configuration without touching the chip.
 *
 * Fails with `InvalidArgument` for an unset or insane `Limits`, a value off the data sheet grid, a
 * reserved code or a value that is not an enumerator; with `OutOfLimits` for a valid value outside
 * `Limits`; with `ReadOnlyRail` for a buck voltage on SW3 or on a buck not declared `Dvs::Enabled`;
 * with `Unsafe` for a safety downgrade without its acknowledgement.
 *
 * @param config The configuration.
 * @return `Error::Ok` or the first failure.
 */
constexpr Error Validate(const Config& config) noexcept {
  if (const Error e = detail::ValidateLimits(config.limits); e != Error::Ok) {
    return e;
  }
  if (const Error e = detail::ValidateOrder(config); e != Error::Ok) {
    return e;
  }
  if (const Error e = detail::ValidateCharger(config); e != Error::Ok) {
    return e;
  }
  if (const Error e = detail::ValidateRails(config); e != Error::Ok) {
    return e;
  }
  if (const Error e = detail::ValidatePeripherals(config); e != Error::Ok) {
    return e;
  }
  return detail::ValidatePower(config);
}

/**
 * @brief A `Limits` for a 4.2 V single cell charged at up to 500 mA from a 1 A supply; a starting
 * point whose numbers the application must check against its own cell and supply.
 * @return The limits; rails are not permitted until their windows are set.
 */
constexpr Limits SingleCellLimits() noexcept {
  Limits l{};
  l.cell_max_mv = 4200;
  l.cell_max_charge_ma = 500;
  l.input_max_ma = 1000;
  return l;
}

static_assert(Validate(Config{.limits = SingleCellLimits()}) == Error::Ok);
static_assert(Validate(Config{}) == Error::InvalidArgument);

/** @} */

} // namespace pf1550

#endif // PF1550_CONFIG_HPP
