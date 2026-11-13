/**
 * @file pf1550_plan.hpp
 * @brief The register writes of each configuration block as pure `constexpr` data.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * A plan is a short list of `{register, mask, value}` entries: the driver reads the register,
 * replaces the bits of `mask` with the bits of `value`, writes it back and reads it again to
 * verify (`PF1550::applyPlan`). Bits outside the mask, reserved and unused bits included, are
 * written back as read. The plans take a `Config` that `Validate()` has accepted; every code is
 * produced by the codecs of `pf1550_codec.hpp`, and the plans are proved with `static_assert`
 * against the register map (KB-CPP20-012).
 */
#ifndef PF1550_PLAN_HPP
#define PF1550_PLAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_codec.hpp"
#include "pf1550_config.hpp"
#include "pf1550_detail.hpp"
#include "pf1550_irq.hpp"
#include "pf1550_registers.hpp"
#include "pf1550_types.hpp"

namespace pf1550::detail {

/// One read-modify-write: the bits of `mask` of register `reg` become the bits of `value`.
struct RegWrite {
  uint8_t reg{0};   ///< Absolute register address.
  uint8_t mask{0};  ///< Bits to replace; 0 means "skip this entry".
  uint8_t value{0}; ///< New bits, already in place (`value & ~mask` must be 0).
};

/// One plan entry.
/// @param reg Absolute register address.
/// @param mask Bits to replace.
/// @param value New bits, already in place.
/// @return The entry.
constexpr RegWrite MakeWrite(uint8_t reg, uint8_t mask, uint8_t value) noexcept {
  return RegWrite{.reg = reg, .mask = mask, .value = value};
}

/// Registers of one rail.
struct RailRegs {
  uint8_t volt;      ///< Run-mode voltage register (0 for VREFDDR).
  uint8_t stby_volt; ///< Standby voltage (bucks only, else 0).
  uint8_t slp_volt;  ///< Sleep voltage (bucks only, else 0).
  uint8_t ctrl;      ///< Control register: EN, STBY_EN, OMODE, LPWR and more.
  uint8_t ctrl1;     ///< SWx_CTRL1: ILIM (bucks only, else 0).
};

/// The registers of each rail, indexed by `Rail`.
inline constexpr std::array<RailRegs, kRailCount> kRailRegs = {{
    {.volt = reg::kSw1Volt,
     .stby_volt = reg::kSw1StbyVolt,
     .slp_volt = reg::kSw1SlpVolt,
     .ctrl = reg::kSw1Ctrl,
     .ctrl1 = reg::kSw1Ctrl1},
    {.volt = reg::kSw2Volt,
     .stby_volt = reg::kSw2StbyVolt,
     .slp_volt = reg::kSw2SlpVolt,
     .ctrl = reg::kSw2Ctrl,
     .ctrl1 = reg::kSw2Ctrl1},
    {.volt = reg::kSw3Volt,
     .stby_volt = reg::kSw3StbyVolt,
     .slp_volt = reg::kSw3SlpVolt,
     .ctrl = reg::kSw3Ctrl,
     .ctrl1 = reg::kSw3Ctrl1},
    {.volt = reg::kLdo1Volt, .stby_volt = 0, .slp_volt = 0, .ctrl = reg::kLdo1Ctrl, .ctrl1 = 0},
    {.volt = reg::kLdo2Volt, .stby_volt = 0, .slp_volt = 0, .ctrl = reg::kLdo2Ctrl, .ctrl1 = 0},
    {.volt = reg::kLdo3Volt, .stby_volt = 0, .slp_volt = 0, .ctrl = reg::kLdo3Ctrl, .ctrl1 = 0},
    {.volt = 0, .stby_volt = 0, .slp_volt = 0, .ctrl = reg::kVrefddrCtrl, .ctrl1 = 0},
}};

/// The power-down order register of each rail, indexed by `Rail`.
inline constexpr std::array<uint8_t, kRailCount> kPwrdnSeqRegs = {
    reg::kSw1PwrdnSeq,  reg::kSw2PwrdnSeq,  reg::kSw3PwrdnSeq,    reg::kLdo1PwrdnSeq,
    reg::kLdo2PwrdnSeq, reg::kLdo3PwrdnSeq, reg::kVrefddrPwrdnSeq};

/// The registers of `rail`.
constexpr const RailRegs& RegsOf(Rail rail) noexcept {
  return kRailRegs[static_cast<std::size_t>(ToUnderlying(rail)) % kRailCount];
}

/// `flag` as `bit` when set.
constexpr uint8_t BitIf(bool flag, uint8_t bit) noexcept {
  return flag ? bit : static_cast<uint8_t>(0U);
}

/// Two bit sets combined.
constexpr uint8_t Or(uint8_t a, uint8_t b) noexcept {
  return static_cast<uint8_t>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

/// The enable bits EN, STBY_EN, OMODE and LPWR of a rail's control register.
inline constexpr uint8_t kModeMask =
    Or(Or(reg::kCtrlEn, reg::kCtrlStbyEn), Or(reg::kCtrlOmode, reg::kCtrlLpwr));

/// The bits of a register that the chip changes by itself, so a read-back after a write that did
/// not name them may differ: the charger mode (a fault drops it), WDTCLR (it does not hold) and the
/// EN bit of a rail (REGSCPEN clears it on a current limit, Table 136).
constexpr uint8_t SelfChangingBits(uint8_t address) noexcept {
  if (address == reg::kChgOper) {
    return reg::ChgOperMode::kMask;
  }
  if (address == reg::kBatfetCnfg) {
    return reg::Wdtclr::kMask;
  }
  for (const RailRegs& r : kRailRegs) {
    if ((r.ctrl == address) && (address != 0U)) {
      return reg::kCtrlEn;
    }
  }
  return 0U;
}

/// A register bit the driver changed and verified: if it reads differently later, the chip reset.
struct Sentinel {
  uint8_t address{0};
  uint8_t mask{0};
  uint8_t value{0};
};

/// The bits a partial reset restores although the chip did not reset: VBUS_LIN_ILIM goes back to
/// its OTP value when VBUSIN becomes invalid (Table 163, reset condition CHGPOK_RSTB), so a cable
/// removal changes it. They are not reset evidence; `CheckChargerAgainstLimits()` and the charge
/// start check the input limit against `Limits` (ambiguity C14).
constexpr uint8_t PartialResetBits(uint8_t address) noexcept {
  return (address == reg::kVbusInlimCnfg) ? reg::VbusLinIlim::kMask : uint8_t{0};
}

/// Whether `address` is an interrupt mask register (their all-ones reset value is legitimate).
constexpr bool IsMaskRegister(uint8_t address) noexcept {
  bool found = false;
  for (const uint8_t m : kIrqMaskRegs) {
    found = found || (m == address);
  }
  return found;
}

/// The enable bits for `modes`.
constexpr uint8_t ModeBits(const RailModes& modes) noexcept {
  return Or(Or(BitIf(modes.run, reg::kCtrlEn), BitIf(modes.standby, reg::kCtrlStbyEn)),
            Or(BitIf(modes.sleep, reg::kCtrlOmode), BitIf(modes.low_power, reg::kCtrlLpwr)));
}

/// The code of a field as `uint8_t` from an enumerator.
template <Enumeration E>
constexpr uint32_t CodeOf(E e) noexcept {
  return static_cast<uint32_t>(ToUnderlying(e));
}

/// The register code of a debounce setting: 0, 2 or 3 (codes 0 and 1 are the same, ambiguity C41).
constexpr uint32_t DebounceCode(Debounce d) noexcept {
  return (d == Debounce::Ms31) ? 0U : (CodeOf(d) + 1U);
}

/// The register code of a short debounce: 1 to 3 (code 0 is reserved).
constexpr uint32_t ShortDebounceCode(ShortDebounce d) noexcept {
  return CodeOf(d) + 1U;
}

/// The control register entry of a buck: enables, forced PWM, discharge and (SW1, SW2) DVS speed.
constexpr RegWrite BuckCtrlWrite(Rail rail, const BuckConfig& b) noexcept {
  const bool dvs_rail = (rail != Rail::Sw3);
  uint8_t mask = Or(kModeMask, Or(reg::kBuckFpwm, Or(reg::kBuckFpwmInDvs, reg::kBuckRdisEnb)));
  uint8_t value = Or(ModeBits(b.modes), Or(BitIf(b.fpwm, reg::kBuckFpwm),
                                           Or(BitIf(b.fpwm_in_dvs, reg::kBuckFpwmInDvs),
                                              BitIf(!b.output_discharge, reg::kBuckRdisEnb))));
  if (!dvs_rail) {
    // SW3_CTRL bits 4 and 5 are UNUSED (Table 125): neither is written.
    mask = static_cast<uint8_t>(static_cast<uint32_t>(mask) &
                                ~static_cast<uint32_t>(reg::kBuckFpwmInDvs));
    value = static_cast<uint8_t>(static_cast<uint32_t>(value) & static_cast<uint32_t>(mask));
  }
  if (dvs_rail) {
    mask = Or(mask, reg::kBuckDvsSpeed);
    value = Or(value, BitIf(b.dvs_speed == DvsSpeed::Us4, reg::kBuckDvsSpeed));
  }
  return MakeWrite(RegsOf(rail).ctrl, mask, value);
}

/// A voltage entry: the code of `uv` in `field`, or a skipped entry when `uv` is 0.
constexpr RegWrite BuckVoltWrite(uint8_t address, Microvolts uv) noexcept {
  if (uv.value == 0U) {
    return {};
  }
  const Result<uint8_t> code = BuckDvsCode(uv.value);
  return MakeWrite(address, reg::BuckVolt6::kMask, static_cast<uint8_t>(code.value));
}

/// The five register entries of a buck block (run, standby, sleep voltage, control, ILIM).
constexpr std::array<RegWrite, 5> BuckPlan(Rail rail, const BuckConfig& b) noexcept {
  const RailRegs& r = RegsOf(rail);
  const bool volts = (b.dvs == Dvs::Enabled) && (rail != Rail::Sw3);
  return {
      {volts ? BuckVoltWrite(r.volt, b.run_uv) : RegWrite{},
       volts ? BuckVoltWrite(r.stby_volt, b.standby_uv) : RegWrite{},
       volts ? BuckVoltWrite(r.slp_volt, b.sleep_uv) : RegWrite{}, BuckCtrlWrite(rail, b),
       MakeWrite(r.ctrl1, reg::BuckIlim::kMask, static_cast<uint8_t>(CodeOf(b.current_limit)))}};
}

/// The two register entries of an LDO block (voltage, enables). The load switch bit is never in a
/// plan.
constexpr std::array<RegWrite, 2> LdoPlan(Rail rail, const LdoConfig& l) noexcept {
  const RailRegs& r = RegsOf(rail);
  RegWrite volt{};
  if (l.volt.value != 0U) {
    const uint8_t mask = (rail == Rail::Ldo2) ? reg::Ldo4Volt::kMask : reg::Ldo5Volt::kMask;
    volt = MakeWrite(r.volt, mask, LdoCode(rail, l.volt.value).value);
  }
  return {{volt, MakeWrite(r.ctrl, kModeMask, ModeBits(l.modes))}};
}

/// The VREFDDR enables.
constexpr RegWrite VrefDdrPlan(const VrefDdrConfig& v) noexcept {
  return MakeWrite(reg::kVrefddrCtrl, kModeMask, ModeBits(v.modes));
}

/// The register entries of the small blocks: VSNVS, coin cell, USBPHY and the two LED registers.
constexpr std::array<RegWrite, 5> PeripheralPlan(const Config& c) noexcept {
  RegWrite vsnvs{};
  RegWrite coin{};
  RegWrite phy{};
  RegWrite led_pwm{};
  RegWrite led_cfg{};
  if (c.vsnvs.apply) {
    vsnvs = MakeWrite(reg::kVsnvsCtrl, reg::kVsnvsLibgdis,
                      BitIf(c.vsnvs.local_bandgap_disable, reg::kVsnvsLibgdis));
  }
  if (c.coin.apply) {
    const uint8_t code = CoinVoltageCode(c.coin.voltage_mv).value;
    coin = MakeWrite(reg::kCoinCellControl, Or(reg::CoinVcoin::kMask, reg::kCoinChen),
                     Or(code, BitIf(c.coin.charge_enable, reg::kCoinChen)));
  }
  if (c.usbphy.apply) {
    phy = MakeWrite(reg::kUsbPhyLdoCnfg, Or(reg::kUsbPhyLdo, Or(reg::kUsbPhy49, reg::kActDisPhy)),
                    Or(BitIf(c.usbphy.enable, reg::kUsbPhyLdo),
                       Or(BitIf(c.usbphy.voltage == UsbPhyVoltage::V4p9, reg::kUsbPhy49),
                          BitIf(c.usbphy.active_discharge, reg::kActDisPhy))));
  }
  if (c.led.apply) {
    const LedConfig& l = c.led;
    led_pwm = MakeWrite(reg::kLedPwm, Or(reg::LedPwmDuty::kMask, Or(reg::kLedRamp, reg::kLedEn)),
                        Or(static_cast<uint8_t>(l.duty_32nds),
                           Or(BitIf(l.ramp, reg::kLedRamp), BitIf(l.enable, reg::kLedEn))));
    led_cfg = MakeWrite(
        reg::kLedCnfg,
        Or(reg::LedFreq::kMask, Or(reg::LedCurrent::kMask, Or(reg::kLedCfg, reg::kLedOvrd))),
        Or(static_cast<uint8_t>(CodeOf(l.frequency)),
           Or(reg::kLedCurrent6mA, Or(BitIf(l.mode == LedMode::FlashWhileCharging, reg::kLedCfg),
                                      BitIf(l.software_override, reg::kLedOvrd)))));
  }
  return {{vsnvs, coin, phy, led_pwm, led_cfg}};
}

/// The register entries of the power control block (PWRCTRL0, 1 and 2).
constexpr std::array<RegWrite, 3> PowerPlan(const PowerConfig& p) noexcept {
  if (!p.apply) {
    return {};
  }
  // Each unguarded field is written only when its own flag is set: a default must not override a
  // value the board chose, and STANDBYINV flips the state with the pin at a steady level.
  uint8_t m0 = p.write_standby ? Or(reg::StandbyDly::kMask, reg::kStandbyInv) : 0_u8;
  uint8_t v0 = p.write_standby ? Or(static_cast<uint8_t>(CodeOf(p.standby_delay)),
                                    BitIf(p.standby_inverted, reg::kStandbyInv))
                               : 0_u8;
  uint8_t m1 = p.write_debounce ? Or(reg::PwronDbnc::kMask, reg::OnkeyDbnc::kMask) : 0_u8;
  uint8_t v1 = p.write_debounce ? Or(static_cast<uint8_t>(DebounceCode(p.pwron_debounce)),
                                     static_cast<uint8_t>(DebounceCode(p.onkey_debounce) << 2))
                                : 0_u8;
  if (p.write_regscp) {
    m1 = Or(m1, reg::kRegScpEn);
    v1 = Or(v1, BitIf(p.ldo_off_on_current_limit, reg::kRegScpEn));
  }
  uint8_t m2 = p.write_low_sys_warn ? reg::LowSysWarn::kMask : 0_u8;
  auto v2 = p.write_low_sys_warn ? static_cast<uint8_t>(CodeOf(p.low_sys_warn) << 2) : 0_u8;
  if (p.write_reset_paths) {
    m0 = Or(m0, reg::TgReset::kMask);
    v0 = Or(v0, static_cast<uint8_t>(CodeOf(p.global_reset) << 6));
    m1 = Or(m1, Or(reg::kPwronRstEn, Or(reg::kRestartEn, reg::kOnkeyRstEn)));
    v1 = Or(v1, Or(BitIf(p.pwron_long_press_reset, reg::kPwronRstEn),
                   Or(BitIf(p.restart_on_long_press, reg::kRestartEn),
                      BitIf(p.onkey_reset, reg::kOnkeyRstEn))));
  }
  if (p.write_uvdet) {
    m2 = Or(m2, reg::Uvdet::kMask);
    v2 = Or(v2, static_cast<uint8_t>(CodeOf(p.uvdet)));
  }
  return {{MakeWrite(reg::kPwrCtrl0, m0, v0), MakeWrite(reg::kPwrCtrl1, m1, v1),
           MakeWrite(reg::kPwrCtrl2, m2, v2)}};
}

/// The seven power-down order registers.
constexpr std::array<RegWrite, kRailCount> PowerDownPlan(const PowerDownSequence& s) noexcept {
  std::array<RegWrite, kRailCount> plan{};
  for (std::size_t i = 0U; i < kRailCount; ++i) {
    plan[i] =
        MakeWrite(kPwrdnSeqRegs[i], reg::PwrdnSeq::kMask,
                  static_cast<uint8_t>(static_cast<uint32_t>(s.order[i]) & reg::PwrdnSeq::kMask));
  }
  return plan;
}

/// The charger entries that `ChargerOperWrite` leaves out: timers, charge limits, thermistor,
/// battery FET, VBUS and the small options. 12 registers.
constexpr std::array<RegWrite, 12> ChargerPlan(const ChargerConfig& c) noexcept {
  const BatfetConfig& b = c.batfet;
  const uint8_t cc = ChargeCurrentCode(c.charge_current_ma).value;
  const uint8_t cv = ChargeVoltageCode(c.charge_voltage_mv).value;
  const uint32_t ieoc = TerminationCode(c.termination_ma).value;
  const uint32_t ilim = InputLimitCode(c.input_limit_ma).value;
  const uint8_t tmr = Or(static_cast<uint8_t>(CodeOf(c.fast_timer)),
                         Or(static_cast<uint8_t>(CodeOf(c.eoc_timer) << 3),
                            BitIf(c.precharge_timer == PrechargeTimer::Min45, reg::kTprechg)));
  const uint8_t tmr_mask = Or(reg::Fchgtime::kMask, Or(reg::Eoctime::kMask, reg::kTprechg));
  const uint8_t fet_mask =
      Or(reg::BatfetOc::kMask, Or(reg::kBovrcDisbatfet, Or(reg::kWdTime, reg::kBovrcNovbus)));
  const uint8_t fet = Or(static_cast<uint8_t>(CodeOf(b.oc_threshold) << 4),
                         Or(BitIf(b.oc_keeps_closed, reg::kBovrcDisbatfet),
                            Or(BitIf(c.watchdog.time == WatchdogTime::S32, reg::kWdTime),
                               BitIf(b.oc_without_vbus, reg::kBovrcNovbus))));
  const uint8_t thm_mask =
      Or(reg::ThmCnfg::kMask,
         Or(reg::Regtemp::kMask, Or(reg::kThmCold, Or(reg::kThmHot, reg::kTempFbEn))));
  const uint8_t thm = Or(static_cast<uint8_t>(CodeOf(c.thermistor)),
                         Or(static_cast<uint8_t>(CodeOf(c.thermal_reg) << 2),
                            Or(BitIf(c.jeita.cold == Cold::CMinus10, reg::kThmCold),
                               Or(BitIf(c.jeita.hot == Hot::C55, reg::kThmHot),
                                  BitIf(c.thermal_foldback, reg::kTempFbEn)))));
  const uint8_t adj_mask =
      Or(reg::kThmWarm, Or(reg::kThmCool, Or(reg::CvAdj::kMask, reg::CcAdj::kMask)));
  const uint8_t adj = Or(Or(BitIf(c.jeita.warm == Warm::C50, reg::kThmWarm),
                            BitIf(c.jeita.cool == Cool::C10, reg::kThmCool)),
                         Or(static_cast<uint8_t>(CodeOf(c.jeita.cv_adjust) << 2),
                            static_cast<uint8_t>(CodeOf(c.jeita.cc_adjust) << 4)));
  const VbusOptions& v = c.vbus;
  return {
      {MakeWrite(reg::kChgTmr, tmr_mask, tmr),
       MakeWrite(reg::kChgEocCnfg, Or(reg::ChgRestart::kMask, reg::Ieoc::kMask),
                 Or(static_cast<uint8_t>(CodeOf(c.restart)), static_cast<uint8_t>(ieoc << 4U))),
       MakeWrite(reg::kChgCurrCnfg, reg::ChgCc::kMask, cc),
       MakeWrite(reg::kBattReg, Or(reg::Chgcv::kMask, reg::VsysMin::kMask),
                 Or(cv, static_cast<uint8_t>(CodeOf(c.vsys_min) << 6))),
       MakeWrite(reg::kBatfetCnfg, fet_mask, fet), MakeWrite(reg::kThmRegCnfg, thm_mask, thm),
       MakeWrite(reg::kVbusInlimCnfg, reg::VbusLinIlim::kMask, static_cast<uint8_t>(ilim << 3U)),
       MakeWrite(reg::kVbusLinDpm, Or(reg::VbusDpmReg::kMask, reg::kVinDpmStop),
                 Or(static_cast<uint8_t>(CodeOf(v.dpm)),
                    BitIf(v.dpm_stop == DpmStop::Mv250, reg::kVinDpmStop))),
       MakeWrite(reg::kDbncDelayTime, Or(reg::VbusOvTdb::kMask, reg::SysWkupDly::kMask),
                 Or(static_cast<uint8_t>(ShortDebounceCode(v.ovlo_debounce)),
                    static_cast<uint8_t>(CodeOf(v.sys_wakeup_delay) << 4))),
       MakeWrite(reg::kChgIntCnfg, Or(reg::kChgIntGen, reg::kEocInt),
                 Or(BitIf(c.interrupts.every_transition, reg::kChgIntGen),
                    BitIf(c.interrupts.eoc, reg::kEocInt))),
       MakeWrite(reg::kThmAdjSetting, adj_mask, adj),
       MakeWrite(
           reg::kVbus2SysCnfg, Or(reg::Vbus2SysTdb::kMask, reg::kVbus2SysThrsh),
           Or(static_cast<uint8_t>(ShortDebounceCode(v.vbus2sys_debounce)),
              BitIf(v.vbus2sys_threshold == Vbus2SysThreshold::Mv175, reg::kVbus2SysThrsh)))}};
}

/// The two charger entries written around the plan: the fault-policy register and CHG_OPER's
/// WDTEN and DISBATFET bits. The mode bits are never in a plan (`PF1550::setChargerMode`).
constexpr std::array<RegWrite, 2> ChargerTailPlan(const ChargerConfig& c) noexcept {
  const FaultPolicy& f = c.batfet.fault_policy;
  const uint8_t flt = Or(Or(BitIf(f.keep_closed_on_overvoltage, reg::kOvfltBfetEn),
                            BitIf(f.keep_closed_on_watchdog, reg::kWdfltBfetEn)),
                         Or(BitIf(f.keep_closed_on_thermistor, reg::kThmsusBfetEn),
                            Or(BitIf(f.keep_closed_on_thermal_shutdown, reg::kTshdnBfetEn),
                               BitIf(f.keep_closed_on_timer_fault, reg::kTmrfltBfetEn))));
  const uint8_t flt_mask =
      Or(reg::kOvfltBfetEn,
         Or(reg::kWdfltBfetEn, Or(reg::kThmsusBfetEn, Or(reg::kTshdnBfetEn, reg::kTmrfltBfetEn))));
  return {{MakeWrite(reg::kFaultBatfetCnfg, flt_mask, flt),
           MakeWrite(reg::kChgOper, Or(reg::kChgOperWdten, reg::kChgOperDisbatfet),
                     Or(BitIf(c.watchdog.enable, reg::kChgOperWdten),
                        BitIf(c.batfet.disable, reg::kChgOperDisbatfet)))}};
}

} // namespace pf1550::detail

#endif // PF1550_PLAN_HPP
