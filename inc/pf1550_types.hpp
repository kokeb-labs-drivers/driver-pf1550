/**
 * @file pf1550_types.hpp
 * @brief Public types of the PF1550 driver: errors, `Result<T>`, enumerations, units, status and
 * interrupt types.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data with no platform dependency. The configuration lives in
 * `pf1550_config.hpp`, the register codecs in `pf1550_codec.hpp`. Enumerator values equal the
 * register codes of the data sheet tables (section 12) wherever the table is contiguous, and every
 * enumeration that a caller can hand in has an `EnumLast` specialisation for `IsEnumerator()`.
 */
#ifndef PF1550_TYPES_HPP
#define PF1550_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_detail.hpp"
#include "pf1550_version.h"

/** @brief The PF1550 driver: everything it declares lives here; internals in `detail`. */
namespace pf1550 {

/**
 * @defgroup pf1550_types Driver types
 * @brief Errors, results, enumerations, status and interrupt types.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  // Base set: the same names and values in every driver of the org (standards/driver.md).
  Ok = 0,              ///< The call succeeded.
  NotInitialized = 1,  ///< `EnsureInitialized()` has not succeeded yet.
  InvalidArgument = 2, ///< A value is not on the data sheet grid, is reserved or not an enumerator;
                       ///< nothing was sent.
  Bus = 3,             ///< A port hook returned `false`.
  Timeout = 4,         ///< A wait for the chip (a DVS step) did not end in the allowed time.
  DeviceIdMismatch = 5, ///< DEVICE_ID did not read 0x7C: wrong chip, no chip or a wiring fault.
  Crc = 6,              ///< Not returned by this driver (I2C transfers carry no CRC).
  Busy = 7,             ///< Not returned by this driver (the chip has no busy state).
  DeviceFault = 8,      ///< Not returned by this driver (faults are reported as `ChargerFault`).
  Unsupported = 9,      ///< The optional port hook this call needs is absent.
  // Chip-specific values start at 16; each one is returned by some call (tests/test_errors.cpp).
  OutOfLimits = 16,  ///< A valid value outside the caller's `Limits`; nothing was sent.
  ReadOnlyRail = 17, ///< SW3, or SW1 or SW2 not declared in DVS mode: its voltage is not written.
  Unsafe = 18,       ///< A safety downgrade without its acknowledgement; nothing was sent.
  Refused = 19,      ///< A precondition of a dangerous call is not met (state, VBUS, ONKEY).
  ChargerFault = 20, ///< The charger reports a fault state; charging is not enabled.
  VerifyFailed = 21, ///< A register read back differently from what was just written.
  NoDevice = 22,     ///< Nothing acknowledged the address: no chip, or it is unpowered.
  DeviceReset = 23,  ///< The chip reset itself (a register the driver changed is back at its
                     ///< start-up value): run `EnsureInitialized()` and `Configure()`.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Timeout"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::OutOfLimits:
    return "OutOfLimits";
  case Error::ReadOnlyRail:
    return "ReadOnlyRail";
  case Error::Unsafe:
    return "Unsafe";
  case Error::Refused:
    return "Refused";
  case Error::ChargerFault:
    return "ChargerFault";
  case Error::VerifyFailed:
    return "VerifyFailed";
  case Error::NoDevice:
    return "NoDevice";
  case Error::DeviceReset:
    return "DeviceReset";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`,
 * with one documented exception: `CheckChargerAgainstLimits()` and the start-up check return the
 * `ChargerAsFound` report in `value` together with `OutOfLimits` or `Unsafe`, because the report is
 * what says why the charger is not covered.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events.
  Debug,     ///< Detail for bring-up.
};

/** @brief A voltage in microvolts, so that grids such as 12.5 mV steps stay exact. */
struct Microvolts {
  uint32_t value{0}; ///< The voltage in microvolts; 0 means "leave as it is" in a configuration.
};

/**
 * @brief Microvolts from millivolts.
 * @param mv Millivolts.
 * @return The voltage.
 */
constexpr Microvolts FromMillivolts(uint32_t mv) noexcept {
  return Microvolts{mv * 1000U};
}

/// Default 7-bit I2C address (section 9.7.1); OTP selects 0x08 to 0x0F.
inline constexpr uint8_t kDefaultAddress = 0x08;
/// The longest the driver waits for one DVS step sequence, in microseconds (64 steps x 4 us).
inline constexpr uint32_t kDvsWaitMaxUs = 256;
/// The longest the application may leave the charger watchdog unserviced when it is on, in seconds
/// (the data sheet gives 80 s, +/-20 %, or 32 s: 16 s covers every reading, ambiguity C8).
inline constexpr uint32_t kWatchdogServiceMaxS = 16;
/// Number of interrupt groups.
inline constexpr std::size_t kIrqGroups = 8;

/** @brief The start-up option of the part (OTP_FLAVOR, Table 87; ordering codes A0 to A9). */
enum class Option : uint8_t {
  A0 = 0,       ///< Not programmed: internal validation only; nothing is assumed about it.
  A1 = 1,       ///< Default option.
  A2 = 2,       ///< Option A2.
  A3 = 3,       ///< Option A3.
  A4 = 4,       ///< Option A4.
  A5 = 5,       ///< Option A5.
  A6 = 6,       ///< Option A6.
  A7 = 7,       ///< Option A7.
  A8 = 8,       ///< Option A8 (edge-sensitive PWRON).
  A9 = 9,       ///< Option A9.
  Unknown = 10, ///< A flavour byte the data sheet does not list (ambiguity C10).
};

/** @brief What `ReadVariant()` reports. */
struct VariantInfo {
  uint8_t device_id{0};           ///< DEVICE_ID (0x7C).
  uint8_t flavor_raw{0};          ///< OTP_FLAVOR as read.
  Option option{Option::Unknown}; ///< The named option for listed values.
  uint8_t silicon_rev_raw{0};     ///< SILICON_REV as read; no decision is made on it (C22).
  uint8_t i2c_slave_addr_code{0}; ///< I2C_ADDR[2:0]: the address is 0x08 plus this.
};

/** @brief STATE_INFO decoded (Table 146). */
enum class PowerState : uint8_t {
  Wait = 0,        ///< 000000.
  Run = 1,         ///< 001100.
  Standby = 2,     ///< 001101.
  Sleep = 3,       ///< 001110.
  RegsDisable = 4, ///< 101011.
  Unknown = 5,     ///< Any other code.
};

/** @brief The power rails the driver configures. */
enum class Rail : uint8_t {
  Sw1 = 0,     ///< Buck 1, 1.0 A, DVS.
  Sw2 = 1,     ///< Buck 2, 1.0 A, DVS.
  Sw3 = 2,     ///< Buck 3, 1.0 A, voltage read only.
  Ldo1 = 3,    ///< LDO1, 300 mA, load switch capable.
  Ldo2 = 4,    ///< LDO2, 400 mA.
  Ldo3 = 5,    ///< LDO3, 300 mA, load switch capable.
  VrefDdr = 6, ///< DDR reference, 10 mA, fixed voltage.
};
/// Number of rails.
inline constexpr std::size_t kRailCount = 7;

/** @brief Whether the OTP configured a buck for dynamic voltage scaling (OTP_SWx_DVS_SEL). */
enum class Dvs : uint8_t {
  Disabled = 0, ///< Non-DVS: the voltage registers are read only and hold the fixed table.
  Enabled = 1,  ///< DVS: voltage is 0.6 V + 12.5 mV x code.
};

/** @brief Buck current limit, typical (SWx_ILIM, Tables 116, 121, 126). */
enum class SwCurrentLimit : uint8_t {
  A1p0 = 0, ///< 1.0 A.
  A1p2 = 1, ///< 1.2 A.
  A1p5 = 2, ///< 1.5 A.
  A2p0 = 3, ///< 2.0 A.
};

/** @brief DVS slew rate (SWx_DVSSPEED). */
enum class DvsSpeed : uint8_t {
  Us2 = 0, ///< 12.5 mV every 2 us.
  Us4 = 1, ///< 12.5 mV every 4 us.
};

/** @brief Charger operating mode (CHG_OPER, Table 156). */
enum class ChargerMode : uint8_t {
  Off = 0,        ///< Charger and linear regulator off.
  LinearOnly = 1, ///< Linear regulator on, no battery charging (reset value).
  Charge = 2,     ///< Charging.
};

/** @brief Fast-charge timer (FCHGTIME, Table 157). */
enum class FastChargeTimer : uint8_t {
  Disabled = 0, ///< No timer: needs the acknowledgement `allow_untimed_charge`.
  Hours2 = 1,   ///< 2 hours.
  Hours4 = 2,   ///< 4 hours (reset value).
  Hours6 = 3,   ///< 6 hours.
  Hours8 = 4,   ///< 8 hours.
  Hours10 = 5,  ///< 10 hours.
  Hours12 = 6,  ///< 12 hours.
  Hours14 = 7,  ///< 14 hours.
};

/** @brief End-of-charge timer (EOCTIME, Table 157): 0 is a 16 s debounce, then 10 min steps. */
enum class EocTimer : uint8_t {
  Debounce16s = 0, ///< 16 s.
  Min10 = 1,       ///< 10 min (reset value).
  Min20 = 2,       ///< 20 min.
  Min30 = 3,       ///< 30 min.
  Min40 = 4,       ///< 40 min.
  Min50 = 5,       ///< 50 min.
  Min60 = 6,       ///< 60 min.
  Min70 = 7,       ///< 70 min.
};

/** @brief Precharge timer (TPRECHG, Table 157). */
enum class PrechargeTimer : uint8_t {
  Min30 = 0, ///< 30 minutes.
  Min45 = 1, ///< 45 minutes.
};

/** @brief Charge restart threshold below CHGCV (CHG_RESTART, Table 158). */
enum class Restart : uint8_t {
  Mv100 = 0,    ///< 100 mV.
  Mv150 = 1,    ///< 150 mV (reset value).
  Mv200 = 2,    ///< 200 mV.
  Disabled = 3, ///< No restart.
};

/** @brief Minimum system voltage (VSYSMIN, Table 160). */
enum class SystemMin : uint8_t {
  Mv3500 = 0, ///< 3.5 V (reset value).
  Mv3700 = 1, ///< 3.7 V.
  Mv4300 = 2, ///< 4.3 V (recommended, section 8.1).
};

/** @brief Thermistor behaviour (THM_CNFG, Table 162). */
enum class Thermistor : uint8_t {
  Ignore = 0,  ///< Not in control: needs the acknowledgement `battery_has_own_ntc_protection`.
  Suspend = 1, ///< Charging stops outside cold and hot (reset value).
  Jeita1 = 2,  ///< Current and voltage reduced in cool and warm.
  Jeita2 = 3,  ///< Current reduced in cool and warm.
};

/** @brief Junction temperature where charge current folds back (REGTEMP, Table 162). */
enum class ThermalRegTemp : uint8_t {
  C80 = 0,  ///< 80 degC.
  C95 = 1,  ///< 95 degC (reset value).
  C110 = 2, ///< 110 degC.
};

/** @brief Charger watchdog period (WD_TIME, Table 161; the table prints 0 twice, ambiguity C8). */
enum class WatchdogTime : uint8_t {
  S80 = 0, ///< 80 s (reset value).
  S32 = 1, ///< 32 s.
};

/** @brief Battery discharge overcurrent threshold (BATFET_OC, Table 161). */
enum class BatfetOc : uint8_t {
  Disabled = 0, ///< No protection: needs the acknowledgement `allow_batfet_oc_off`.
  A2p2 = 1,     ///< 2.2 A typical.
  A2p8 = 2,     ///< 2.8 A typical.
  A3p2 = 3,     ///< 3.2 A typical (reset value).
};

/** @brief Cold threshold (THM_COLD). */
enum class Cold : uint8_t { C0 = 0, CMinus10 = 1 }; ///< 0 degC or -10 degC.
/** @brief Hot threshold (THM_HOT). */
enum class Hot : uint8_t { C60 = 0, C55 = 1 }; ///< 60 degC or 55 degC.
/** @brief Cool threshold (THM_COOL). */
enum class Cool : uint8_t { C15 = 0, C10 = 1 }; ///< 15 degC or 10 degC.
/** @brief Warm threshold (THM_WARM). */
enum class Warm : uint8_t { C45 = 0, C50 = 1 }; ///< 45 degC or 50 degC.

/** @brief JEITA charge voltage reduction (CV_ADJ, Table 168). */
enum class CvAdjust : uint8_t { Mv60 = 0, Mv100 = 1, Mv160 = 2, Mv200 = 3 }; ///< Reduction in mV.
/** @brief JEITA charge current as a share of IFC (CC_ADJ, Table 168). */
enum class CcAdjust : uint8_t { Pct25 = 0, Pct50 = 1, Pct75 = 2, Pct100 = 3 }; ///< Share of IFC.

/** @brief VBUS input regulation (DPM) voltage (VBUS_DPM_REG, Table 164): 3.9 V + 100 mV x code. */
enum class DpmVoltage : uint8_t {
  Mv3900 = 0, ///< 3.9 V (reset value).
  Mv4000 = 1, ///< 4.0 V.
  Mv4100 = 2, ///< 4.1 V.
  Mv4200 = 3, ///< 4.2 V.
  Mv4300 = 4, ///< 4.3 V.
  Mv4400 = 5, ///< 4.4 V.
  Mv4500 = 6, ///< 4.5 V.
  Mv4600 = 7, ///< 4.6 V.
};

/** @brief DPM panic stop threshold (VIN_DPM_STOP). */
enum class DpmStop : uint8_t { Mv200 = 0, Mv250 = 1 }; ///< 200 mV or 250 mV.

/** @brief Short debounce times (VBUS_OV_TDB, VBUS2SYS_TDB; code 0 is reserved and not offered). */
enum class ShortDebounce : uint8_t {
  Us100 = 0, ///< 100 us (register code 1).
  Ms1 = 1,   ///< 1 ms (register code 2).
  Ms10 = 2,  ///< 10 ms (register code 3).
};

/** @brief System wake-up delay (SYS_WKUP_DLY, Table 166). */
enum class WakeupDelay : uint8_t { Ms8 = 0, Ms16 = 1, Ms32 = 2, Ms100 = 3 }; ///< Delay in ms.

/** @brief VBUS to VSYS comparator threshold (VBUS2SYS_THRSH). */
enum class Vbus2SysThreshold : uint8_t { Mv50 = 0, Mv175 = 1 }; ///< 50 mV or 175 mV.

/** @brief USBPHY LDO voltage (USBPHY, Table 165). */
enum class UsbPhyVoltage : uint8_t { V3p3 = 0, V4p9 = 1 }; ///< 3.3 V or 4.9 V.

/** @brief LED PWM frequency (LED_FREQ, Table 172). */
enum class LedFrequency : uint8_t { Hz1 = 0, Hz0p5 = 1, Hz256 = 2, Hz8 = 3 }; ///< Frequency.

/** @brief LED mode (LED_CFG). */
enum class LedMode : uint8_t {
  OnWhileCharging = 0,    ///< On while charging, flashing on a fault, off when done.
  FlashWhileCharging = 1, ///< Flashing while charging, on on a fault, off when done.
};

/** @brief STANDBY pin delay in 32 kHz cycles (STANDBYDLY, Table 135). */
enum class StandbyDelay : uint8_t { None = 0, Cycles1 = 1, Cycles2 = 2, Cycles3 = 3 }; ///< Delay.

/** @brief Button debounce (PWRONDBNC, ONKEYDBNC; codes 00 and 01 are equal, ambiguity C41). */
enum class Debounce : uint8_t {
  Ms31 = 0,  ///< 31.25 ms falling, 31.25 ms rising (register code 0).
  Ms125 = 1, ///< 125 ms falling (register code 2).
  Ms750 = 2, ///< 750 ms falling (register code 3).
};

/** @brief ONKEY hold time for a global reset (TGRESET, Table 135). */
enum class GlobalReset : uint8_t { S4 = 0, S8 = 1, S12 = 2, S16 = 3 }; ///< Seconds.

/** @brief VSYS undervoltage detect, rising threshold (UVDET, Table 137). */
enum class Uvdet : uint8_t { Mv2650 = 0, Mv2800 = 1, Mv3000 = 2, Mv3100 = 3 }; ///< Rising mV.

/** @brief Low system voltage warning, rising threshold (LOW_SYS_WARN, Table 137). */
enum class LowSysWarn : uint8_t { Mv3300 = 0, Mv3500 = 1, Mv3700 = 2, Mv3900 = 3 }; ///< Rising mV.

/** @brief What to do when the charger is found on and unsafe at start-up. */
enum class AsFoundPolicy : uint8_t {
  Report = 0,   ///< Return the error and write nothing (default).
  MakeSafe = 1, ///< Write CHG_OPER = linear only, then verify.
};

/** @brief Charger state (CHG_SNS[3:0], Table 154). */
enum class ChargerState : uint8_t {
  Precharge = 0,          ///< Battery below VPRECHG.LB.
  FastChargeCc = 1,       ///< Constant current.
  FastChargeCv = 2,       ///< Constant voltage.
  EndOfCharge = 3,        ///< Current below IEOC, tEOC running.
  Done = 4,               ///< Charge complete.
  TimerFault = 6,         ///< Timer fault: only off then on exits it.
  ThermistorSuspend = 7,  ///< Battery temperature outside the window.
  Off = 8,                ///< Input invalid or charger disabled.
  BatteryOvervoltage = 9, ///< Battery overvoltage.
  ThermalShutdown = 10,   ///< Junction above TSHDN.
  LinearOnly = 12,        ///< Linear regulator only, not charging.
  Unknown = 255,          ///< A reserved code: reported as a fault, never as charging.
};

/** @brief Battery state (BATT_SNS[2:0], Table 155). */
enum class BatteryState : uint8_t {
  NoValidVbus = 0,               ///< No valid input.
  BelowPrecharge = 1,            ///< VBATT below VPRECHG.LB.
  CannotChargePastPrecharge = 2, ///< Timer fault in precharge.
  AbovePrecharge = 4,            ///< Above the precharge threshold.
  Overvoltage = 5,               ///< Battery overvoltage with a valid input.
  NotDetected = 6,               ///< No battery with a valid input after wake-up.
  Unknown = 255,                 ///< A reserved code.
};

/** @brief The fault the charger reports, classified from the state and flag registers. */
enum class Fault : uint8_t {
  None = 0,               ///< No fault.
  TimerFault = 1,         ///< CHG_SNS 6.
  ThermistorSuspend = 2,  ///< CHG_SNS 7.
  ThermalShutdown = 3,    ///< CHG_SNS 10.
  BatteryOvervoltage = 4, ///< CHG_SNS 9 or BATT_SNS 5.
  BatteryOvercurrent = 5, ///< BATTOC_SNS.
  WatchdogExpired = 6,    ///< WDT_SNS.
  BatteryMissing = 7,     ///< BATT_SNS 6.
  UnknownState = 8,       ///< A reserved CHG_SNS or BATT_SNS code.
};

/** @brief VBUS flags of VBUS_SNS (Table 153). */
struct VbusFlags {
  bool valid{false};  ///< VBUS_VALID.
  bool ovlo{false};   ///< VBUS_OVLO_SNS.
  bool uvlo{false};   ///< VBUS_UVLO_SNS.
  bool in2sys{false}; ///< VBUS_IN2SYS_SNS.
  bool dpm{false};    ///< VBUS_DPM_SNS.
};

/** @brief The charger status of one `ReadChargerStatus()` pass (separate reads, not atomic). */
struct ChargerStatus {
  ChargerState state{ChargerState::Unknown};   ///< CHG_SNS[3:0].
  BatteryState battery{BatteryState::Unknown}; ///< BATT_SNS[2:0].
  bool watchdog_expired{false};                ///< WDT_SNS.
  bool thermistor_outside{false};              ///< THM_SNS.
  bool thermal_regulation{false};              ///< TREG_SNS: charge current is folded back.
  bool battery_overcurrent{false};             ///< BATTOC_SNS.
  VbusFlags vbus{};                            ///< VBUS_SNS.
  Fault fault{Fault::UnknownState};            ///< The classified fault.
  bool consistent{false};                      ///< CHG_SNS read twice with equal values.
};

/** @brief Interrupt groups, one status/mask/sense register triple each. */
enum class IrqGroup : uint8_t {
  Charger = 0,    ///< CHG_INT (0x80).
  SwLowSide = 1,  ///< SW_INT_STAT0.
  SwHighSide = 2, ///< SW_INT_STAT1.
  SwDvs = 3,      ///< SW_INT_STAT2.
  Ldo = 4,        ///< LDO_INT_STAT0.
  Temp = 5,       ///< TEMP_INT_STAT0.
  Onkey = 6,      ///< ONKEY_INT_STAT0.
  Misc = 7,       ///< MISC_INT_STAT0.
};

/** @brief Every interrupt source of Tables 90 to 110 and 150 (31 sources). */
enum class IrqSource : uint8_t {
  ChgSupplement = 0, ///< SUP_I.
  ChgBat2Soc,        ///< BAT2SOC_I.
  ChgBattery,        ///< BAT_I.
  ChgCharger,        ///< CHG_I.
  ChgVbus,           ///< VBUS_I.
  ChgVbusDpm,        ///< VBUS_DPM_I.
  ChgThermistor,     ///< THM_I.
  Sw1LowSideLimit,   ///< SW1_LS_I.
  Sw2LowSideLimit,   ///< SW2_LS_I.
  Sw3LowSideLimit,   ///< SW3_LS_I.
  Sw1HighSideLimit,  ///< SW1_HS_I.
  Sw2HighSideLimit,  ///< SW2_HS_I.
  Sw3HighSideLimit,  ///< SW3_HS_I.
  Sw1DvsDone,        ///< SW1_DVS_DONE_I.
  Sw2DvsDone,        ///< SW2_DVS_DONE_I.
  Ldo1Fault,         ///< LDO1_FAULTI.
  Ldo2Fault,         ///< LDO2_FAULTI.
  Ldo3Fault,         ///< LDO3_FAULTI.
  Temp110,           ///< THERM110I.
  Temp125,           ///< THERM125I.
  OnkeyPush,         ///< PUSHI.
  Onkey1s,           ///< 1SI.
  Onkey2s,           ///< 2SI.
  Onkey3s,           ///< 3SI.
  Onkey4s,           ///< 4SI.
  Onkey8s,           ///< 8SI.
  PowerUp,           ///< PWRUP_I.
  PowerDown,         ///< PWRDN_I.
  PwronChange,       ///< PWRON_I.
  LowSysWarning,     ///< LOW_SYS_WARN_I.
  SysOvervoltage,    ///< SYS_OVLO_I.
};
/// Number of interrupt sources.
inline constexpr std::size_t kIrqSourceCount = 31;

/**
 * @brief Interrupt masks, one byte per group; a set bit masks the source. The default masks every
 * source, as the chip does after power-up (section 9.4).
 */
struct IrqMask {
  std::array<uint8_t, kIrqGroups> masked{0xFF, 0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF, 0xFF}; ///< Per group.
};

/** @brief Interrupt events returned by `ServiceInterrupts()`, one status byte per group. */
struct InterruptEvents {
  std::array<uint8_t, kIrqGroups> status{}; ///< The status bytes as read, before any clear.
  uint8_t categories{0};                    ///< INT_CATEGORY as read first.
  uint8_t passes{0};                        ///< Passes made (at most 3).
  bool fault_class{false};                  ///< A fault-class source is set (see `HasFault()`).
};

/** @brief Which category-summary bits are set in INT_CATEGORY (Table 89). */
struct Categories {
  bool charger{false}; ///< CHG_INT.
  bool sw1{false};     ///< SW1_INT.
  bool sw2{false};     ///< SW2_INT.
  bool sw3{false};     ///< SW3_INT.
  bool ldo{false};     ///< LDO_INT.
  bool onkey{false};   ///< ONKEY_INT.
  bool temp{false};    ///< TEMP_INT.
  bool misc{false};    ///< MISC_INT.
};

/** @brief The registers of the power-down sequence (Tables 139 to 145). */
struct PowerDownSequence {
  /// Order values 0 to 7 per rail, indexed by `Rail`: regulators turn off in descending order; all
  /// zero bypasses the sequencer (section 10.7).
  std::array<uint8_t, kRailCount> order{};
};

/** @brief One rail as the chip holds it. */
struct RailReadout {
  bool enabled_run{false};     ///< EN.
  bool enabled_standby{false}; ///< STBY_EN.
  bool enabled_sleep{false};   ///< OMODE.
  bool low_power{false};       ///< LPWR.
  bool load_switch{false};     ///< LS_EN (LDO1 and LDO3).
  Microvolts volt{}; ///< The run-mode voltage; 0 for VREFDDR or a non-DVS buck of unknown mode.
};

/** @brief Charger registers as found at start-up (the as-found check). */
struct ChargerAsFound {
  uint8_t chg_oper{0};                ///< CHG_OPER raw.
  ChargerMode mode{ChargerMode::Off}; ///< Decoded mode.
  uint32_t charge_voltage_mv{0};      ///< CHGCV decoded.
  uint32_t charge_current_ma{0};      ///< CHG_CC decoded (0 for a reserved code).
  uint32_t input_limit_ma{0};         ///< VBUS_LIN_ILIM decoded (0 for a reserved code).
  FastChargeTimer fast_timer{FastChargeTimer::Disabled}; ///< FCHGTIME.
  Thermistor thermistor{Thermistor::Ignore};             ///< THM_CNFG.
  BatfetOc batfet_oc{BatfetOc::Disabled};                ///< BATFET_OC.
  bool disbatfet{false};                                 ///< DISBATFET: the battery path is off.
  bool safe{false};                                      ///< Within limits and acknowledged.
};

/** @brief A read-only view of the chip for diagnostics (`ReadSnapshot()`). */
struct Snapshot {
  VariantInfo variant{};                       ///< Identity.
  PowerState state{PowerState::Unknown};       ///< STATE_INFO.
  ChargerStatus charger{};                     ///< Charger status.
  std::array<uint8_t, kIrqGroups> sense{};     ///< SENSE registers (live, not latched).
  std::array<RailReadout, kRailCount> rails{}; ///< The rails.
  PowerDownSequence power_down{};              ///< PWRDN_SEQ registers.
  uint8_t por_delay_code{0};                   ///< PWRCTRL0.POR_DLY (RESETBMCU delay).
};

/** @brief Result of `SelfCheck()`: what is wrong, if anything. */
struct SelfCheckReport {
  bool identity_ok{false};         ///< DEVICE_ID read 0x7C.
  bool state_run{false};           ///< STATE_INFO is Run.
  bool rails_as_configured{false}; ///< Every configured rail reads enabled as configured.
  bool no_latched_fault{false}; ///< No fault-class interrupt status and the charger reports none.
  bool input_limit_ok{false};   ///< VBUS_LIN_ILIM is valid, within Limits and as configured.
};

/** @brief Token: the caller confirms that `EnterShip()` cuts all supplies, VSNVS included. */
struct ShipConfirm {};
/** @brief Token: the caller confirms that `EnterCoreOff()` turns the PMIC core off. */
struct CoreOffConfirm {};
/**
 * @brief Token: the caller confirms that a load switch has no current limit and cannot go back, and
 * states the voltage of the LDO's input rail. The call refuses when that voltage is outside the
 * rail's `Limits` window: in switch mode the output is the input voltage (section 7.2.6).
 */
struct LoadSwitchConfirm {
  uint32_t input_mv{0}; ///< The input rail voltage of the LDO, millivolts.
};

namespace detail {
/// Last enumerators, for `IsEnumerator()`.
template <>
struct EnumLast<Option> {
  static constexpr Option kValue = Option::Unknown; ///< The last enumerator.
};
/// Last enumerator of `Rail`.
template <>
struct EnumLast<Rail> {
  static constexpr Rail kValue = Rail::VrefDdr; ///< The last enumerator.
};
/// Last enumerator of `Dvs`.
template <>
struct EnumLast<Dvs> {
  static constexpr Dvs kValue = Dvs::Enabled; ///< The last enumerator.
};
/// Last enumerator of `SwCurrentLimit`.
template <>
struct EnumLast<SwCurrentLimit> {
  static constexpr SwCurrentLimit kValue = SwCurrentLimit::A2p0; ///< The last enumerator.
};
/// Last enumerator of `DvsSpeed`.
template <>
struct EnumLast<DvsSpeed> {
  static constexpr DvsSpeed kValue = DvsSpeed::Us4; ///< The last enumerator.
};
/// Last enumerator of `ChargerMode`.
template <>
struct EnumLast<ChargerMode> {
  static constexpr ChargerMode kValue = ChargerMode::Charge; ///< The last enumerator.
};
/// Last enumerator of `FastChargeTimer`.
template <>
struct EnumLast<FastChargeTimer> {
  static constexpr FastChargeTimer kValue = FastChargeTimer::Hours14; ///< The last enumerator.
};
/// Last enumerator of `EocTimer`.
template <>
struct EnumLast<EocTimer> {
  static constexpr EocTimer kValue = EocTimer::Min70; ///< The last enumerator.
};
/// Last enumerator of `PrechargeTimer`.
template <>
struct EnumLast<PrechargeTimer> {
  static constexpr PrechargeTimer kValue = PrechargeTimer::Min45; ///< The last enumerator.
};
/// Last enumerator of `Restart`.
template <>
struct EnumLast<Restart> {
  static constexpr Restart kValue = Restart::Disabled; ///< The last enumerator.
};
/// Last enumerator of `SystemMin`.
template <>
struct EnumLast<SystemMin> {
  static constexpr SystemMin kValue = SystemMin::Mv4300; ///< The last enumerator.
};
/// Last enumerator of `Thermistor`.
template <>
struct EnumLast<Thermistor> {
  static constexpr Thermistor kValue = Thermistor::Jeita2; ///< The last enumerator.
};
/// Last enumerator of `ThermalRegTemp`.
template <>
struct EnumLast<ThermalRegTemp> {
  static constexpr ThermalRegTemp kValue = ThermalRegTemp::C110; ///< The last enumerator.
};
/// Last enumerator of `WatchdogTime`.
template <>
struct EnumLast<WatchdogTime> {
  static constexpr WatchdogTime kValue = WatchdogTime::S32; ///< The last enumerator.
};
/// Last enumerator of `BatfetOc`.
template <>
struct EnumLast<BatfetOc> {
  static constexpr BatfetOc kValue = BatfetOc::A3p2; ///< The last enumerator.
};
/// Last enumerator of `Cold`.
template <>
struct EnumLast<Cold> {
  static constexpr Cold kValue = Cold::CMinus10; ///< The last enumerator.
};
/// Last enumerator of `Hot`.
template <>
struct EnumLast<Hot> {
  static constexpr Hot kValue = Hot::C55; ///< The last enumerator.
};
/// Last enumerator of `Cool`.
template <>
struct EnumLast<Cool> {
  static constexpr Cool kValue = Cool::C10; ///< The last enumerator.
};
/// Last enumerator of `Warm`.
template <>
struct EnumLast<Warm> {
  static constexpr Warm kValue = Warm::C50; ///< The last enumerator.
};
/// Last enumerator of `CvAdjust`.
template <>
struct EnumLast<CvAdjust> {
  static constexpr CvAdjust kValue = CvAdjust::Mv200; ///< The last enumerator.
};
/// Last enumerator of `CcAdjust`.
template <>
struct EnumLast<CcAdjust> {
  static constexpr CcAdjust kValue = CcAdjust::Pct100; ///< The last enumerator.
};
/// Last enumerator of `DpmVoltage`.
template <>
struct EnumLast<DpmVoltage> {
  static constexpr DpmVoltage kValue = DpmVoltage::Mv4600; ///< The last enumerator.
};
/// Last enumerator of `DpmStop`.
template <>
struct EnumLast<DpmStop> {
  static constexpr DpmStop kValue = DpmStop::Mv250; ///< The last enumerator.
};
/// Last enumerator of `ShortDebounce`.
template <>
struct EnumLast<ShortDebounce> {
  static constexpr ShortDebounce kValue = ShortDebounce::Ms10; ///< The last enumerator.
};
/// Last enumerator of `WakeupDelay`.
template <>
struct EnumLast<WakeupDelay> {
  static constexpr WakeupDelay kValue = WakeupDelay::Ms100; ///< The last enumerator.
};
/// Last enumerator of `Vbus2SysThreshold`.
template <>
struct EnumLast<Vbus2SysThreshold> {
  static constexpr Vbus2SysThreshold kValue = Vbus2SysThreshold::Mv175; ///< The last enumerator.
};
/// Last enumerator of `UsbPhyVoltage`.
template <>
struct EnumLast<UsbPhyVoltage> {
  static constexpr UsbPhyVoltage kValue = UsbPhyVoltage::V4p9; ///< The last enumerator.
};
/// Last enumerator of `LedFrequency`.
template <>
struct EnumLast<LedFrequency> {
  static constexpr LedFrequency kValue = LedFrequency::Hz8; ///< The last enumerator.
};
/// Last enumerator of `LedMode`.
template <>
struct EnumLast<LedMode> {
  static constexpr LedMode kValue = LedMode::FlashWhileCharging; ///< The last enumerator.
};
/// Last enumerator of `StandbyDelay`.
template <>
struct EnumLast<StandbyDelay> {
  static constexpr StandbyDelay kValue = StandbyDelay::Cycles3; ///< The last enumerator.
};
/// Last enumerator of `Debounce`.
template <>
struct EnumLast<Debounce> {
  static constexpr Debounce kValue = Debounce::Ms750; ///< The last enumerator.
};
/// Last enumerator of `GlobalReset`.
template <>
struct EnumLast<GlobalReset> {
  static constexpr GlobalReset kValue = GlobalReset::S16; ///< The last enumerator.
};
/// Last enumerator of `Uvdet`.
template <>
struct EnumLast<Uvdet> {
  static constexpr Uvdet kValue = Uvdet::Mv3100; ///< The last enumerator.
};
/// Last enumerator of `LowSysWarn`.
template <>
struct EnumLast<LowSysWarn> {
  static constexpr LowSysWarn kValue = LowSysWarn::Mv3900; ///< The last enumerator.
};
/// Last enumerator of `IrqGroup`.
template <>
struct EnumLast<IrqGroup> {
  static constexpr IrqGroup kValue = IrqGroup::Misc; ///< The last enumerator.
};
/// Last enumerator of `AsFoundPolicy`.
template <>
struct EnumLast<AsFoundPolicy> {
  static constexpr AsFoundPolicy kValue = AsFoundPolicy::MakeSafe; ///< The last enumerator.
};
} // namespace detail

/** @return The driver version, e.g. `"0.1.0"`, from the generated `pf1550_version.h`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return PF1550_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return PF1550_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return PF1550_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return PF1550_VERSION_PATCH;
}

/** @} */

} // namespace pf1550

#endif // PF1550_TYPES_HPP
