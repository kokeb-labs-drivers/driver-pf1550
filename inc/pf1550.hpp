/**
 * @file pf1550.hpp
 * @brief PF1550 driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `pf1550::PF1550<Port>` drives one NXP PF1550 power management IC (a single-cell linear charger,
 * three buck regulators, three LDOs, VSNVS, VREFDDR, a USB PHY LDO, an LED driver and a coin cell
 * charger) through a platform port you write (see `pf1550_i2c_interface.hpp`). The driver has no
 * platform code, allocates nothing, throws nothing, takes no lock and is not thread-safe: give each
 * object one owner, or guard it with one mutex.
 *
 * The chip is a power supply and the driver treats it as one: there is no raw register write, every
 * value is checked against the data sheet grids and against the `Limits` your application declares,
 * every safety downgrade needs a named acknowledgement, and every typed configuration write is a
 * read-modify-write from two agreeing reads that is read back and compared (the command writes of
 * `ServiceInterrupts()`, the shutdown requests and the watchdog service excepted; see
 * docs/contract.md).
 */
#ifndef PF1550_HPP
#define PF1550_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "pf1550_codec.hpp"
#include "pf1550_config.hpp"
#include "pf1550_detail.hpp"
#include "pf1550_i2c_interface.hpp"
#include "pf1550_irq.hpp"
#include "pf1550_plan.hpp"
#include "pf1550_registers.hpp"
#include "pf1550_sequencer.hpp"
#include "pf1550_types.hpp"
#include "pf1550_version.h"

namespace pf1550 {

namespace detail {

/// A zero of exactly the type the status latch stores (MISRA C++:2023 Rule 7.0.6).
inline constexpr uint8_t kByteZero = 0_u8;

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept the exact
// signature, `noexcept` and the result type; the driver rejects a hook that is declared but does
// not match (KB-CPP20-007).

/// The port declares something called `GpioSet`.
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Standby, GpioSignal::ACTIVE); };

/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `GpioGet`.
template <typename Port>
concept DeclaresGpioGet = requires(Port& port, bool& level) { port.GpioGet(CtrlPin::Intb, level); };

/// The port reads control pins: `bool GpioGet(CtrlPin, bool& asserted) noexcept`.
template <typename Port>
concept HasGpioGet =
    requires { ExactHook<Port, bool(CtrlPin, bool&) noexcept>::Take(&Port::GpioGet); };

/// The port declares something called `GetLastFailure`.
template <typename Port>
concept DeclaresGetLastFailure =
    requires(Port& port, I2cFailure& why) { port.GetLastFailure(why); };

/// The port reports why a transfer failed: `bool GetLastFailure(I2cFailure&) noexcept`.
template <typename Port>
concept HasGetLastFailure =
    requires { ExactHook<Port, bool(I2cFailure&) noexcept>::Take(&Port::GetLastFailure); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`. A narrower parameter would truncate waits.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the DVS wait is a requirement, never skipped.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "pf1550: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log(Port& port, [[maybe_unused]] LogLevel level,
         [[maybe_unused]] const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    (void)port;
  }
}

/// STATE_INFO register to `PowerState`.
[[nodiscard]] constexpr PowerState StateOf(uint8_t raw) noexcept {
  return DecodePowerState(reg::StateInfo::Extract(raw));
}

/// CHG_SNS[3:0] to `ChargerState`; a reserved code is `Unknown`.
[[nodiscard]] constexpr ChargerState DecodeChargerState(uint32_t code) noexcept {
  switch (code) {
  case 0U:
  case 1U:
  case 2U:
  case 3U:
  case 4U:
  case 6U:
  case 7U:
  case 8U:
  case 9U:
  case 10U:
  case 12U:
    return static_cast<ChargerState>(code);
  default:
    break;
  }
  return ChargerState::Unknown;
}

/// BATT_SNS[2:0] to `BatteryState`; a reserved code is `Unknown`.
[[nodiscard]] constexpr BatteryState DecodeBatteryState(uint32_t code) noexcept {
  switch (code) {
  case 0U:
  case 1U:
  case 2U:
  case 4U:
  case 5U:
  case 6U:
    return static_cast<BatteryState>(code);
  default:
    break;
  }
  return BatteryState::Unknown;
}

/// The fault the charger reports for these states and flags; `Fault::None` when it reports none.
[[nodiscard]] constexpr Fault ClassifyFault(ChargerState state, BatteryState battery, bool watchdog,
                                            bool battery_oc) noexcept {
  if (state == ChargerState::ThermalShutdown) {
    return Fault::ThermalShutdown;
  }
  if (state == ChargerState::TimerFault) {
    return Fault::TimerFault;
  }
  if ((state == ChargerState::BatteryOvervoltage) || (battery == BatteryState::Overvoltage)) {
    return Fault::BatteryOvervoltage;
  }
  if (battery_oc) {
    return Fault::BatteryOvercurrent;
  }
  if (watchdog) {
    return Fault::WatchdogExpired;
  }
  if (battery == BatteryState::NotDetected) {
    return Fault::BatteryMissing;
  }
  if (state == ChargerState::ThermistorSuspend) {
    return Fault::ThermistorSuspend;
  }
  if ((state == ChargerState::Unknown) || (battery == BatteryState::Unknown)) {
    return Fault::UnknownState;
  }
  return Fault::None;
}

/// VBUS_SNS decoded.
[[nodiscard]] constexpr VbusFlags DecodeVbus(uint8_t raw) noexcept {
  const auto bits = static_cast<uint32_t>(raw);
  VbusFlags f{};
  f.valid = (bits & reg::kVbusValid) != 0U;
  f.ovlo = (bits & reg::kVbusOvlo) != 0U;
  f.uvlo = (bits & reg::kVbusUvlo) != 0U;
  f.in2sys = (bits & reg::kVbusIn2Sys) != 0U;
  f.dpm = (bits & reg::kVbusDpm) != 0U;
  return f;
}

static_assert((StateOf(0x0C_u8) == PowerState::Run) && (StateOf(0xCC_u8) == PowerState::Run));
static_assert((DecodeChargerState(5_u32) == ChargerState::Unknown) &&
              (DecodeChargerState(12_u32) == ChargerState::LinearOnly));
static_assert(ClassifyFault(ChargerState::Off, BatteryState::NoValidVbus, false, false) ==
              Fault::None);
static_assert(ClassifyFault(ChargerState::Unknown, BatteryState::AbovePrecharge, false, false) ==
              Fault::UnknownState);
static_assert(ClassifyFault(ChargerState::TimerFault, BatteryState::Unknown, false, false) ==
              Fault::TimerFault);
static_assert(DecodeVbus(reg::kVbusValid).valid && !DecodeVbus(reg::kVbusValid).uvlo);

} // namespace detail

/**
 * @defgroup pf1550_driver PF1550 driver
 * @brief The driver class.
 * @{
 */

// clang-format off: the table below must keep one row per line.
/**
 * @brief Driver for one PF1550 on a platform port.
 *
 * Lifecycle: construct with the configuration (no I/O), then `EnsureInitialized()`, the one start-up
 * call: it validates the configuration, brings the bus up, checks DEVICE_ID and checks the charger
 * the chip starts with against your `Limits`. `Configure()` then writes the blocks you asked for.
 * Every call that touches the chip returns `Error::NotInitialized` until `EnsureInitialized()` has
 * succeeded; no call starts the chip as a side effect.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `void DelayUs(uint32_t us) noexcept` | The DVS-done wait | Compile error at the first call that waits |
 * | `bool GpioSet(CtrlPin, GpioSignal) noexcept` | STANDBY pin: `RequestStandby()` | `RequestStandby()` returns `Unsupported` |
 * | `bool GpioGet(CtrlPin, bool&) noexcept` | INTB: `IntbAsserted()`, idle skip of `ServiceInterrupts()` | `IntbAsserted()` returns `Unsupported` |
 * | `bool GetLastFailure(I2cFailure&) noexcept` | Tell an absent chip (`NoDevice`) from a bus error (`Bus`) | Every failure is `Bus` |
 * | `void Log(LogLevel, const char*) noexcept` | One line per lifecycle event or failure | Nothing is logged |
 *
 * A hook declared with the wrong signature is a compile error.
 *
 * @tparam Port Your class derived from `I2cInterface<Port>`.
 */
// clang-format on
template <I2cPort Port>
class PF1550 {
  static_assert(
      !detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
      "pf1550: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) noexcept`");
  static_assert(
      !detail::DeclaresGpioGet<Port> || detail::HasGpioGet<Port>,
      "pf1550: the optional GpioGet hook must be `bool GpioGet(CtrlPin, bool&) noexcept`");
  static_assert(!detail::DeclaresGetLastFailure<Port> || detail::HasGetLastFailure<Port>,
                "pf1550: the optional GetLastFailure hook must be `bool "
                "GetLastFailure(I2cFailure&) noexcept`");
  static_assert(!detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
                "pf1550: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`");
  static_assert(
      !detail::DeclaresLog<Port> || detail::HasLog<Port>,
      "pf1550: the optional Log hook must be `void Log(LogLevel, const char* message) noexcept`");

public:
  /**
   * @brief Bind the driver to a port and a 7-bit I2C address, and store the configuration. No I/O,
   * cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config What the driver validates and writes; checked in `EnsureInitialized()`, before
   * any I/O. `Config{}` has no `Limits` and is rejected there.
   * @param address 7-bit I2C address, 0x08 to 0x0F (OTP selects it, section 9.7.1).
   */
  explicit PF1550(Port& port, const Config& config, uint8_t address = kI2cAddress) noexcept
      : bus_(port, address), config_(config) {}

  /** @brief One driver object per chip. */
  PF1550(const PF1550&) = delete;
  /** @brief One driver object per chip. */
  PF1550& operator=(const PF1550&) = delete;
  /** @brief The driver holds a reference to the port. */
  ~PF1550() noexcept = default;

  /**
   * @name Lifecycle
   * @{
   */
  /**
   * @brief Start the driver: validate the configuration, bring the bus up, check DEVICE_ID = 0x7C
   * and check the charger as found against `Limits` (the chip's start-up
   * options turn the charger on with the fast-charge timer disabled, Table 85).
   *
   * Sends: DEVICE_ID, OTP_FLAVOR (the DVS declaration of SW1 and SW2 is checked against Table 85
   * for a listed option), then
   * the charger registers. It writes nothing otherwise. With `AsFoundPolicy::MakeSafe` and an
   * unsafe charger found on, also one write of CHG_OPER (linear regulator only) and the same reads
   * again.
   *
   * After DEVICE_ID has passed, `SetChargerMode(Off or LinearOnly)`, `CheckChargerAgainstLimits()`
   * and `CancelShutdownRequest()` work even when this call then fails (an unsafe charger found on
   * must always be switchable off).
   *
   * @return `Ok`; the `Validate()` error; `Bus` or `NoDevice` (nothing acknowledged: needs the
   * optional `GetLastFailure()` hook); `DeviceIdMismatch`; `Refused` (a DVS declaration that
   * contradicts the part), `Unsafe` (a DVS declaration on an unlisted part without its
   * acknowledgement); `OutOfLimits` or `Unsafe` for an unsafe charger under
   * `AsFoundPolicy::Report`; `VerifyFailed`. The call can be repeated.
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after `EnsureInitialized()` has succeeded and until a chip reset is seen. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /** @return The stored configuration. */
  [[nodiscard]] const Config& GetConfig() const noexcept {
    return config_;
  }

  /**
   * @brief Write every block of the configuration whose `apply` is true: rails in `rail_order`, the
   * small blocks, power control, the power-down order, the interrupt masks and last the charger.
   *
   * The charger is written with its mode at "linear regulator only", then every parameter, then
   * WDTEN and DISBATFET, and CHG_OPER's mode last (datasheet silent on glitch-free changes: C16,
   * C49). Every register write is read back.
   *
   * @return `Ok`, `NotInitialized`, `Bus`, `VerifyFailed`, `Timeout` (a DVS step did not finish) or
   *         `ChargerFault` (parameters are written, charging is not enabled).
   */
  Result<void> Configure() noexcept;
  /** @} */

  /**
   * @name Identity and state (read only)
   * @{
   */
  /** @return DEVICE_ID, OTP_FLAVOR, SILICON_REV and I2C_ADDR[2:0] (Tables 86 to 88, 147); never an
   *  error for an option the data sheet does not list. Option A0 parts report their raw bytes only.
   *  Table 87 lists only flavour bytes 0x01 to 0x03 (it ends with "continues"), so A4 to A9 are
   *  reported as `Option::Unknown` with the raw byte; pass such an option by hand to
   *  `StartupProfileOf()` when you know your part (C10).
   */
  Result<VariantInfo> ReadVariant() noexcept;
  /** @return STATE_INFO decoded (Table 146). */
  Result<PowerState> ReadPowerState() noexcept;
  /** @return A diagnostic view: identity, state, charger status, SENSE registers, rails, power-down
   *  order and POR_DLY. About 60 single-register reads. */
  Result<Snapshot> ReadSnapshot() noexcept;
  /** @return Identity, state Run, rails enabled as configured, no latched fault and the input limit
   *  the chip holds (`input_limit_ok`: a valid code, within `Limits.input_max_ma`, and the
   * configured one when the charger block is applied). Read only. */
  Result<SelfCheckReport> SelfCheck() noexcept;
  /** @return The charger as the chip holds it, checked against `Limits` and the acknowledgements.
   *  `OutOfLimits` or `Unsafe` when the charger is on and its settings are not covered. */
  Result<ChargerAsFound> CheckChargerAgainstLimits() noexcept;
  /** @} */

  /**
   * @name Charger
   * @{
   */
  /** @brief One pass over VBUS_SNS, CHG_SNS (twice), BATT_SNS: states, flags and the classified
   * fault. The input limit is checked as well (as in `ServiceChargerWatchdog()`, whether or not
   * `watchdog.enable` is set): a cable removal puts it back to its OTP value, and with the charger
   * on the configured limit is written again.
   * A failed check returns its error and not the status (the status is not kept; `ReadSnapshot()`
   * reads it without the check).
   * @return The status, or `OutOfLimits` (the limit is above `Limits` and the charger was switched
   * off), `VerifyFailed`, `DeviceReset`, `Bus`, `NotInitialized`. */
  Result<ChargerStatus> ReadChargerStatus() noexcept;
  /**
   * @brief Change the charger mode. `Off` and `LinearOnly` are always allowed once DEVICE_ID has
   * passed (also when `EnsureInitialized()` failed or the chip reset). `Charge` first checks the
   * configuration and the chip's charger registers against `Limits` and refuses with
   * `ChargerFault` while the charger reports a fault.
   * @param mode The mode.
   * @return `Ok`, `InvalidArgument`, `ChargerFault`, `OutOfLimits`, `Unsafe`, `VerifyFailed`,
   * `DeviceReset` (`Charge` only), `Bus`.
   */
  Result<void> SetChargerMode(ChargerMode mode) noexcept;
  /** @brief Leave a latched fault state: off (needs only an identified chip), verify, charge (needs
   * a live driver: `DeviceReset` or `NotInitialized` after the off step), at most
   * `kMaxChargerRestarts` times in a row without the charger reaching a charging state (`Refused`
   * after that).
   *  @return `Ok`, `Refused`, `ChargerFault`, `OutOfLimits`, `Unsafe`, `VerifyFailed`, `Bus`. */
  Result<void> RestartCharger() noexcept;
  /** @brief Clear the charger watchdog (WDTCLR = 1). With `watchdog.enable` call it every
   *  `kWatchdogServiceMaxS` seconds or less (the limit check below also runs from
   *  `ReadChargerStatus()` and `ServiceInterrupts()`). The register is written from two agreeing
   * reads; a read that shows battery overcurrent protection off is refused unless
   *  `allow_batfet_oc_off` is set (a stuck line must not switch the protection off). The other
   *  bits are compared after the write; WDTCLR itself is not held by the chip (C58). A cable
   * removal puts the input limit back to its OTP value (Table 163) while the charger keeps its
   * mode: with the charger in Charge (or LinearOnly, when the charger block is applied) the limit
   * is read as well, and a limit that is not the configured one is written again once VBUSIN is
   * valid (while it is invalid the field is held in reset: nothing is written, `SelfCheck()`
   * reports `input_limit_ok` false), a limit above `Limits.input_max_ma` that cannot be (the
   * charger block is not applied) switches the charger off and returns `OutOfLimits`.
   *  @return `Ok`, `Unsafe`, `OutOfLimits`, `VerifyFailed`, `DeviceReset`, `Bus`, `NotInitialized`.
   */
  Result<void> ServiceChargerWatchdog() noexcept;
  /** @} */

  /**
   * @name Rails
   * @{
   */
  /** @brief Read a rail.
   *  @param rail Any rail.
   *  @return The rail's enables, load switch and voltage as the chip holds them. SW1 and SW2 decode
   * by the `dvs` you declared in `Config`. */
  Result<RailReadout> ReadRail(Rail rail) noexcept;
  /**
   * @brief Change which states a rail is on in.
   * @param rail Any rail.
   * @param modes EN, STBY_EN, OMODE, LPWR; checked against `Limits` (the rail needs a window) and
   *        `load_below_10ma`. `standby` or `sleep` without `run` is refused: EN = 0 disables the
   *        rail in every state (Table 42). Before a rail is enabled the voltage registers it will
   *        use are read and decoded; a voltage outside the rail's window is `OutOfLimits`.
   *        A request that leaves every state off needs only an identified chip (no reset check,
   *        no limits) and is written even when the register cannot be read: the other bits then
   *        go out as zero and only the switched bits are read back (`VerifyFailed` if they differ).
   * @return `Ok`, `InvalidArgument`, `OutOfLimits`, `Unsafe`, `DeviceReset`, `VerifyFailed`, `Bus`.
   */
  Result<void> SetRailModes(Rail rail, const RailModes& modes) noexcept;
  /**
   * @brief Set a rail's run-mode voltage. SW1 and SW2 only when declared `Dvs::Enabled`; waits for
   * the DVS-done event (at most `kDvsWaitMaxUs`).
   * @param rail SW1, SW2, LDO1, LDO2 or LDO3.
   * @param uv On the rail's grid and inside its `Limits` window.
   * @return `Ok`, `InvalidArgument`, `OutOfLimits`, `ReadOnlyRail` (SW3, or a buck not declared
   * DVS), `Timeout`, `VerifyFailed`, `Bus`.
   */
  Result<void> SetRailVoltage(Rail rail, Microvolts uv) noexcept;
  /**
   * @brief Turn LDO1 or LDO3 into a load switch. IRREVERSIBLE: the chip cannot go back until the
   * next power cycle (a 1 to 0 write of LS_EN "is not allowed", Tables 130 and 134), and a load
   * switch has no current limit (section 7.2.6): the output is the input rail voltage.
   * @param rail `Rail::Ldo1` or `Rail::Ldo3`.
   * @param confirm The caller's confirmation, with the input rail voltage in millivolts: it must
   *        lie inside the rail's `Limits` window.
   * @return `Ok`, `InvalidArgument`, `OutOfLimits` (no window, or the input voltage is outside it),
   * `Refused` (the rail is in low-power mode), `DeviceReset`, `VerifyFailed`, `Bus`.
   */
  Result<void> EnableLoadSwitch(Rail rail, LoadSwitchConfirm confirm) noexcept;
  /** @} */

  /**
   * @name Power states and the sequencer
   * @{
   */
  /** @brief Drive the STANDBY pin (needs `GpioSet()`); `Unsupported` without it.
   *  @param standby true to request STANDBY.
   *  @return `Ok`, `Unsupported`, `Bus`. */
  Result<void> RequestStandby(bool standby) noexcept;
  /**
   * @brief Request SHIP mode (GOTO_SHIP): every supply, VSNVS included, turns off.
   *
   * Refused unless STATE_INFO is Run, Standby or Sleep and VBUS reads below UVLO and not valid. The
   * chip enters SHIP only when VBUSIN is below 1.0 V, which the driver cannot measure (Table 83,
   * ambiguity C6); with VBUS between 1 V and UVLO it ends in REGS_DISABLE instead, and GOTO_SHIP
   * stays set (the bit is plain read/write, Table 138): it acts again at the next REGS_DISABLE.
   * `CancelShutdownRequest()` clears it. Recovery from SHIP is by a charger or a battery attach.
   * Not verified on hardware.
   * @param confirm The caller's confirmation that every supply turns off.
   * @return `Ok`, `Refused`, `Bus`.
   */
  Result<void> EnterShip(ShipConfirm confirm) noexcept;
  /**
   * @brief Arm CORE_OFF (GOTO_CORE_OFF). The bit stays set; it takes effect when the part is in
   * REGS_DISABLE (VBUS invalid, ONKEY high; Table 83 I and footnotes 8 and 9, section 10.2.2): in
   * Run, Standby or Sleep alone nothing happens, and a later REGS_DISABLE acts on it. Refused
   * unless STATE_INFO is Run, Standby or Sleep, VBUS reads invalid and ONKEY reads high.
   * `CancelShutdownRequest()` disarms it. Not verified on hardware.
   * @param confirm The caller's confirmation that the PMIC core turns off.
   * @return `Ok`, `Refused`, `DeviceReset`, `Bus`.
   */
  Result<void> EnterCoreOff(CoreOffConfirm confirm) noexcept;
  /**
   * @brief Clear GOTO_SHIP and GOTO_CORE_OFF (write 0 to PWRCTRL3) and read it back. Works after
   * DEVICE_ID has passed, whatever else failed: switching a request off is never refused.
   * @return `Ok`, `NotInitialized` (DEVICE_ID has not passed yet), `VerifyFailed` (a command bit
   * is still set), `Bus`.
   */
  Result<void> CancelShutdownRequest() noexcept;
  /** @brief Read the power-down order.
   *  @return The seven `*_PWRDN_SEQ` registers (section 10.7). */
  Result<PowerDownSequence> ReadPowerDownSequence() noexcept;
  /**
   * @brief Write the power-down order. Needs `Acknowledgements::power_down_order_reviewed`: the
   * order decides which supply drops first and a wrong order can damage a processor.
   * @param sequence Order 0 to 7 per rail; regulators turn off in descending order, all 0 bypasses.
   * @return `Ok`, `InvalidArgument`, `Unsafe`, `VerifyFailed`, `Bus`.
   */
  Result<void> SetPowerDownSequence(const PowerDownSequence& sequence) noexcept;
  /** @} */

  /**
   * @brief Read one documented register as it is, with no decoding: a read-only escape hatch for
   * bench work. One transfer; no register is written. A status register (INT_STAT, "Read or
   * Write 1 to clear", Tables 90 to 110) may clear on the read: the byte is also ORed into the
   * driver's latch (`GetLatchedEvents()`), so `ServiceInterrupts()` still reports it; a non-zero
   * CHG_INT byte also makes the input limit check of the next `ServiceInterrupts()` owed, as if
   * that call had read it.
   * @param address An absolute register address (PMIC 0x00 to 0x6F, charger 0x80 to 0x9F).
   * @return The byte, `InvalidArgument` for an address the data sheet does not document,
   *         `NotInitialized` or `Bus`.
   */
  Result<uint8_t> ReadRegister(uint8_t address) noexcept;
  /**
   * @name Interrupts
   * @{
   */
  /** @brief Write the interrupt masks (1 masks a source). Reserved bits are preserved.
   *  @param mask One mask byte per group.
   *  @return `Ok`, `VerifyFailed`, `Bus`. */
  Result<void> SetIrqMask(const IrqMask& mask) noexcept;
  /** @brief Read INT_CATEGORY.
   *  @return INT_CATEGORY decoded. */
  Result<Categories> ReadCategories() noexcept;
  /** @brief Read the live SENSE register of a group.
   *  @param group The group.
   *  @return The sense byte (not latched), or `InvalidArgument`. */
  Result<uint8_t> ReadSense(IrqGroup group) noexcept;
  /**
   * @brief Read and clear the pending interrupts: INT_CATEGORY, then each flagged group's status
   * byte (copied out before the clear), then a write of the same bits to clear them, at most three
   * passes. The result also holds every status bit that another call read since the last service
   * (the latch). With `GpioGet()`, an interrupt mask that masks no source, INTB not asserted, an
   * empty latch and no input limit check pending, the call returns an empty result without bus
   * traffic.
   * A status event that arrives between the read and the clearing write is cleared too when the
   * chip clears on read (C3); keep the mask narrow. A charger interrupt also starts the input limit
   * check of `ServiceChargerWatchdog()`. The check is owed from the moment a charger status
   * byte is read, by this call or by `ReadRegister()`, so a call that fails later (in another
   * pass, or a clear write) leaves it pending too; it stays pending until it has passed (it also
   * keeps the INTB shortcut away, whatever `ClearLatchedEvents()` did) and is made again by the
   * next call. The flag stays across `DeviceReset` and a new `EnsureInitialized()` (one extra
   * check, harmless). A restore is not written over a chip that reset meanwhile (`DeviceReset`).
   * @return The status bytes as read (PWRUP_I is an event like the others), `OutOfLimits` (the
   * limit is above `Limits`, the charger was switched off), `VerifyFailed`, `DeviceReset`, `Bus`,
   * `NotInitialized`. After one of the errors the bits stay in the latch for the next call.
   */
  Result<InterruptEvents> ServiceInterrupts() noexcept;
  /** @brief The status bits read by any call since the last `ClearLatchedEvents()` or service.
   *  No I/O.
   *  @return The latched status bytes; `fault_class` set when one is a fault-class source. */
  [[nodiscard]] InterruptEvents GetLatchedEvents() const noexcept {
    InterruptEvents e{};
    e.status = latch_;
    e.fault_class = HasFault(latch_);
    return e;
  }
  /** @brief Empty the latch. No I/O. */
  void ClearLatchedEvents() noexcept {
    latch_.fill(detail::kByteZero);
  }
  /** @brief Read INTB.
   *  @return true when INTB is asserted (low); `Unsupported` without `GpioGet()`. */
  Result<bool> IntbAsserted() noexcept;
  /** @} */

  /// Most restarts of a latched charger fault in a row before `RestartCharger()` refuses.
  static constexpr uint32_t kMaxChargerRestarts = 3U;

private:
  Result<uint8_t> rd(uint8_t address) noexcept;
  Result<uint8_t> rdChecked(uint8_t address) noexcept;
  Result<void> wr(uint8_t address, uint8_t value) noexcept;
  Result<void> rmw(uint8_t address, uint8_t current, uint8_t mask, uint8_t value,
                   uint8_t unchecked) noexcept;
  Result<void> update(uint8_t address, uint8_t mask, uint8_t value) noexcept;
  Result<void> applyPlan(std::span<const detail::RegWrite> plan) noexcept;
  Error busError() noexcept;
  Result<void> fail(Error error, const char* message) noexcept;
  Result<void> requireInitialized() const noexcept;
  Result<void> requireIdentified() const noexcept;
  Result<void> requireLive() noexcept;
  void noteStatus(std::size_t group, uint8_t value) noexcept;
  Result<void> checkReset() noexcept;
  void recordSentinel(uint8_t address, uint8_t changed, uint8_t expected) noexcept;
  Result<void> updateOff(uint8_t address, uint8_t mask, uint8_t value) noexcept;
  Result<void> checkDeclaredDvs() noexcept;
  Result<void> checkHeldVoltage(Rail rail, const RailModes& modes, bool run_written,
                                bool standby_written, bool sleep_written) noexcept;
  [[nodiscard]] bool dvsOf(Rail rail) const noexcept;
  [[nodiscard]] bool latchEmpty() const noexcept;
  /// Whether the VBUS_INLIM_CNFG byte holds a limit the supply tolerates: a valid code, at most
  /// `Limits.input_max_ma`, and the configured one when the charger block is applied.
  [[nodiscard]] bool inputLimitOk(uint8_t raw) const noexcept;
  /// With the charger on (Charge or LinearOnly): the input limit is as configured or inside
  /// `Limits`, else the configured one is written again, or (not configured) the charger is
  /// switched off.
  Result<void> checkInputLimit() noexcept;

  Result<void> identify() noexcept;
  Result<ChargerAsFound> readChargerAsFound(bool as_if_on) noexcept;
  Result<void> makeSafe() noexcept;
  Result<void> setMode(ChargerMode mode) noexcept;

  Result<void> applyBlocks() noexcept;
  Result<void> applyRail(Rail rail) noexcept;
  Result<void> applyBuck(Rail rail, const BuckConfig& b) noexcept;
  Result<void> applyLdo(Rail rail, const LdoConfig& l) noexcept;
  Result<void> applyCharger() noexcept;
  [[nodiscard]] const BuckConfig* buckConfig(Rail rail) const noexcept;
  [[nodiscard]] const RailModes* modesOf(Rail rail) const noexcept;
  Result<bool> needsDvsWait(Rail rail, Microvolts uv) noexcept;
  Result<void> writeVoltage(Rail rail, Microvolts uv) noexcept;
  Result<void> waitDvsDone(Rail rail) noexcept;
  Result<void> clearDvsDone(Rail rail) noexcept;
  Result<bool> railsAsConfigured() noexcept;
  Result<bool> noLatchedFault() noexcept;
  /// `ReadChargerStatus()` without the input limit check (the self-check reports it instead).
  Result<ChargerStatus> readChargerStatus() noexcept;
  Result<VbusFlags> readVbus() noexcept;
  Result<void> requireSystemOn() noexcept;
  Result<void> setPwrCtrl3(uint8_t bit) noexcept;
  Result<std::array<RailReadout, kRailCount>> readRails() noexcept;
  Result<void> collectStatus(InterruptEvents& events, uint8_t categories) noexcept;
  /// INTB is high, no source is masked, nothing is latched and no limit check is pending.
  [[nodiscard]] bool intbIdle() noexcept;
  /// INT_CATEGORY and the flagged status bytes, at most three passes.
  Result<void> collectPasses(InterruptEvents& events) noexcept;
  /// The pending input limit check of a charger interrupt (see `ServiceInterrupts()`).
  Result<void> runPendingLimitCheck() noexcept;

  /// Polls of the DVS-done flag; with `kDvsPollUs` they span `kDvsWaitMaxUs`.
  static constexpr uint32_t kDvsPollUs = 16U;

  static constexpr std::size_t kSentinels = 4U;

  detail::RegisterBus<Port> bus_;
  Config config_{};
  std::array<detail::Sentinel, kSentinels> sentinels_{};
  std::size_t newest_{0};
  uint32_t restarts_{0};
  std::array<uint8_t, kIrqGroups> latch_{}; ///< Status bits read by any call, until serviced.
  std::array<bool, 2> dvs_{};               ///< Effective DVS mode of SW1 and SW2.
  bool identified_{false};                  ///< DEVICE_ID has passed.
  bool limit_pending_{false}; ///< A charger interrupt's input limit check has not passed yet.
  bool intb_complete_{false}; ///< Every source is unmasked: INTB high means idle.
  bool initialized_{false};
};

/** @} */

} // namespace pf1550

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/pf1550.ipp" // IWYU pragma: export

#endif // PF1550_HPP
