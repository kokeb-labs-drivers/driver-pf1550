/**
 * @file pf1550.ipp
 * @brief PF1550 driver implementation, included by `pf1550.hpp`.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * Section numbers refer to the PF1550 datasheet, Rev. 7.
 */
#ifndef PF1550_IPP
#define PF1550_IPP

namespace pf1550 {

namespace detail {

/// The charger status of the four raw reads (`chg_again` is the second CHG_SNS read).
[[nodiscard]] constexpr ChargerStatus BuildChargerStatus(uint8_t vbus, uint8_t chg, uint8_t batt,
                                                         uint8_t chg_again) noexcept {
  ChargerStatus s{};
  const uint32_t state_code = reg::ChgSnsState::Extract(chg);
  s.state = DecodeChargerState(state_code);
  s.battery = DecodeBatteryState(reg::BattSnsState::Extract(batt));
  // Table 154 marks 0x0B reserved while 8.9.2 reports a watchdog expiry there (ambiguity C9):
  // either reading counts as expired.
  s.watchdog_expired =
      ((static_cast<uint32_t>(chg) & reg::kChgSnsWdt) != 0U) || (state_code == 11U);
  s.thermistor_outside = (static_cast<uint32_t>(chg) & reg::kChgSnsThm) != 0U;
  s.thermal_regulation = (static_cast<uint32_t>(chg) & reg::kChgSnsTreg) != 0U;
  s.battery_overcurrent = (static_cast<uint32_t>(batt) & reg::kBattSnsOc) != 0U;
  s.vbus = DecodeVbus(vbus);
  s.consistent = (chg == chg_again);
  s.fault = ClassifyFault(s.state, s.battery, s.watchdog_expired, s.battery_overcurrent);
  return s;
}

/// Whether a charger state means the charger is working (clears the restart counter).
[[nodiscard]] constexpr bool IsCharging(ChargerState state) noexcept {
  switch (state) {
  case ChargerState::Precharge:
  case ChargerState::FastChargeCc:
  case ChargerState::FastChargeCv:
  case ChargerState::EndOfCharge:
  case ChargerState::Done:
    return true;
  case ChargerState::TimerFault:
  case ChargerState::ThermistorSuspend:
  case ChargerState::Off:
  case ChargerState::BatteryOvervoltage:
  case ChargerState::ThermalShutdown:
  case ChargerState::LinearOnly:
  case ChargerState::Unknown:
  default:
    break;
  }
  return false;
}

/// The charger mode a CHG_OPER register value stands for; the reserved code 3 counts as `Charge`
/// (the worst case for the checks).
[[nodiscard]] constexpr ChargerMode ModeOf(uint8_t chg_oper) noexcept {
  const uint32_t code = reg::ChgOperMode::Extract(chg_oper);
  if (code == 0U) {
    return ChargerMode::Off;
  }
  return (code == 1U) ? ChargerMode::LinearOnly : ChargerMode::Charge;
}

static_assert(BuildChargerStatus(0_u8, 0x0B_u8, 0_u8, 0x0B_u8).watchdog_expired);
static_assert(BuildChargerStatus(0_u8, 0x08_u8, 0_u8, 0x08_u8).fault == Fault::None);
static_assert((ModeOf(0x03_u8) == ChargerMode::Charge) &&
              (ModeOf(0x01_u8) == ChargerMode::LinearOnly));

/// The voltage a rail's voltage register stands for, microvolts (0 when the rail has none).
[[nodiscard]] constexpr uint32_t VoltUv(Rail rail, uint8_t raw, bool dvs) noexcept {
  switch (rail) {
  case Rail::Sw1:
  case Rail::Sw2: {
    const uint32_t code = reg::BuckVolt6::Extract(raw);
    return dvs ? BuckDvsUv(code) : BuckFixedUvOf(code);
  }
  case Rail::Sw3:
    return Sw3Uv(reg::BuckVolt4::Extract(raw));
  case Rail::Ldo1:
  case Rail::Ldo3:
    return LdoUv(rail, reg::Ldo5Volt::Extract(raw));
  case Rail::Ldo2:
    return LdoUv(rail, reg::Ldo4Volt::Extract(raw));
  case Rail::VrefDdr:
  default:
    break;
  }
  return 0U;
}

/// The verdict of the as-found check on a charger that is on: out of limits, unsafe, or fine.
[[nodiscard]] constexpr Error AsFoundVerdict(const ChargerAsFound& f, bool codes_ok,
                                             const Limits& l, const Acknowledgements& a) noexcept {
  const bool over = (f.charge_voltage_mv > l.cell_max_mv) ||
                    (f.charge_current_ma > l.cell_max_charge_ma) ||
                    (f.input_limit_ma > l.input_max_ma);
  if (!codes_ok || over) {
    return Error::OutOfLimits;
  }
  const bool unsafe =
      Unacknowledged(f.fast_timer == FastChargeTimer::Disabled, a.allow_untimed_charge) ||
      Unacknowledged(f.thermistor == Thermistor::Ignore, a.battery_has_own_ntc_protection) ||
      Unacknowledged(f.batfet_oc == BatfetOc::Disabled, a.allow_batfet_oc_off) ||
      Unacknowledged(f.disbatfet, a.allow_batfet_off);
  return unsafe ? Error::Unsafe : Error::Ok;
}

/// A read of a protection register that shows a protection off, in bits a call does not name and
/// the application did not acknowledge, is not written back: a line stuck at 0 reads like that,
/// and two equal reads do not prove the chip holds it.
[[nodiscard]] constexpr bool ReadShowsUnackedOff(uint8_t address, uint8_t value, uint8_t mask,
                                                 const Acknowledgements& a) noexcept {
  const auto named = [mask](uint8_t field) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(field)) != 0U;
  };
  switch (address) {
  case reg::kBatfetCnfg:
    return (reg::BatfetOc::Extract(value) == 0U) && !a.allow_batfet_oc_off &&
           !named(reg::BatfetOc::kMask);
  case reg::kChgOper:
    return ((static_cast<uint32_t>(value) & reg::kChgOperDisbatfet) != 0U) && !a.allow_batfet_off &&
           !named(reg::kChgOperDisbatfet);
  case reg::kChgTmr:
    return (reg::Fchgtime::Extract(value) == 0U) && !a.allow_untimed_charge &&
           !named(reg::Fchgtime::kMask);
  case reg::kThmRegCnfg:
    return (reg::ThmCnfg::Extract(value) == 0U) && !a.battery_has_own_ntc_protection &&
           !named(reg::ThmCnfg::kMask);
  default:
    break;
  }
  return false;
}

} // namespace detail

// -------------------------------------------------------------------------------------------------
// Register access
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Error PF1550<Port>::busError() noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, "pf1550: bus transfer failed");
  if constexpr (detail::HasGetLastFailure<Port>) {
    I2cFailure why = I2cFailure::None;
    if (bus_.GetPort().GetLastFailure(why) && (why == I2cFailure::AddressNack)) {
      return Error::NoDevice;
    }
  }
  return Error::Bus;
}

template <I2cPort Port>
Result<void> PF1550<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, message);
  return Result<void>::Failure(error);
}

template <I2cPort Port>
Result<void> PF1550<Port>::requireInitialized() const noexcept {
  return initialized_ ? Result<void>::Success() : Result<void>::Failure(Error::NotInitialized);
}

template <I2cPort Port>
Result<void> PF1550<Port>::requireIdentified() const noexcept {
  return identified_ ? Result<void>::Success() : Result<void>::Failure(Error::NotInitialized);
}

/// A call that changes the chip needs a driver that has started and has not seen the chip reset: a
/// reset returns the charger to its start-up state (Table 85) and clears what `Configure()` wrote.
template <I2cPort Port>
Result<void> PF1550<Port>::requireLive() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return r;
  }
  return checkReset();
}

template <I2cPort Port>
void PF1550<Port>::noteStatus(std::size_t group, uint8_t value) noexcept {
  const std::size_t g = group % kIrqGroups;
  latch_[g] = static_cast<uint8_t>(static_cast<uint32_t>(latch_[g]) | static_cast<uint32_t>(value));
  // A charger status bit read here may be cleared by that read (C3), by the service's clear write,
  // or dropped from the latch by `ClearLatchedEvents()`: whoever read it, the input limit check is
  // owed from now on, until a service has passed it (`ReadRegister()` and `ServiceInterrupts()`
  // alike, and a call that fails after the read).
  if ((g == static_cast<std::size_t>(detail::ToUnderlying(IrqGroup::Charger))) && (value != 0U)) {
    limit_pending_ = true;
  }
}

/// The chip has no reset flag the data sheet defines (PWRUP_I is the STANDBY or SLEEP to RUN
/// completion flag, Table 108, so a standby wake-up sets it too). A reset is seen the way hardware
/// lessons 12a asks: a register the driver changed away from its start-up value is read back and
/// compared with what the driver wrote. A bit that is back at its start-up value after a verified
/// write means the chip reloaded its OTP image (UVDET, a watchdog reset, a REGS_DISABLE pulse).
template <I2cPort Port>
void PF1550<Port>::recordSentinel(uint8_t address, uint8_t changed, uint8_t expected) noexcept {
  const auto keep = static_cast<uint32_t>(changed);
  for (std::size_t i = 0U; i < kSentinels; ++i) {
    detail::Sentinel& s = sentinels_[i];
    if ((s.mask != 0U) && (s.address == address)) {
      s.mask = static_cast<uint8_t>(static_cast<uint32_t>(s.mask) | keep);
      s.value = static_cast<uint8_t>((static_cast<uint32_t>(s.value) & ~keep) |
                                     (static_cast<uint32_t>(expected) & keep));
      newest_ = i;
      return;
    }
  }
  newest_ = (newest_ + 1U) % kSentinels;
  sentinels_[newest_] = detail::Sentinel{.address = address, .mask = changed, .value = expected};
}

template <I2cPort Port>
Result<void> PF1550<Port>::checkReset() noexcept {
  // Every recorded register is read (at most `kSentinels`): a reset restores all of them, and a
  // record whose value was later written back to the start-up value shows nothing on its own.
  for (const detail::Sentinel& mark : sentinels_) {
    if (mark.mask == 0U) {
      continue;
    }
    // A difference is believed when a second read shows it again (a glitch is not a reset).
    bool same = false;
    for (uint8_t attempt = 0U; (attempt < 2U) && !same; ++attempt) {
      const Result<uint8_t> v = rd(mark.address);
      if (!v) {
        return Result<void>::Failure(v.error);
      }
      same = ((static_cast<uint32_t>(v.value) ^ static_cast<uint32_t>(mark.value)) &
              static_cast<uint32_t>(mark.mask)) == 0U;
    }
    if (!same) {
      initialized_ = false;
      sentinels_.fill(detail::Sentinel{});
      return fail(Error::DeviceReset, "pf1550: the chip has reset, run EnsureInitialized()");
    }
  }
  return Result<void>::Success();
}

template <I2cPort Port>
Result<uint8_t> PF1550<Port>::rd(uint8_t address) noexcept {
  std::array<uint8_t, 1> value{};
  if (bus_.Read(address, std::span<uint8_t>{value}) != Error::Ok) {
    return Result<uint8_t>::Failure(busError());
  }
  return Result<uint8_t>::Success(value[0]);
}

template <I2cPort Port>
Result<uint8_t> PF1550<Port>::rdChecked(uint8_t address) noexcept {
  // A value that is written back into the chip must be one the chip really holds: two reads in a
  // row agree (at most three reads), and a register other than an interrupt mask does not read all
  // ones, which is what a floating or stuck line gives (hardware lessons 4 and 12).
  constexpr uint8_t kExtraReads = 2U;
  Result<uint8_t> prev = rd(address);
  for (uint8_t i = 0U; i < kExtraReads; ++i) {
    if (!prev) {
      return prev;
    }
    const Result<uint8_t> next = rd(address);
    if (!next) {
      return next;
    }
    if (next.value == prev.value) {
      if ((next.value == 0xFFU) && !detail::IsMaskRegister(address)) {
        detail::Log(bus_.GetPort(), LogLevel::Error, "pf1550: a register reads all ones");
        return Result<uint8_t>::Failure(Error::VerifyFailed);
      }
      return next;
    }
    prev = next;
  }
  detail::Log(bus_.GetPort(), LogLevel::Error, "pf1550: repeated reads of a register differ");
  return Result<uint8_t>::Failure(Error::VerifyFailed);
}

template <I2cPort Port>
Result<void> PF1550<Port>::wr(uint8_t address, uint8_t value) noexcept {
  // No call reaches a register that is read only, reserved, a key or undocumented (section 12).
  if (!reg::IsWritable(address)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const std::array<uint8_t, 1> data{value};
  if (bus_.Write(address, std::span<const uint8_t>{data}) != Error::Ok) {
    return Result<void>::Failure(busError());
  }
  return Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::update(uint8_t address, uint8_t mask, uint8_t value) noexcept {
  // The typed setter reaches configuration and mask registers only: a status register is cleared
  // by the interrupt service, a command register is written by the shutdown calls.
  const reg::Access access = reg::AccessOf(address);
  if ((access == reg::Access::Status) || (access == reg::Access::Command)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const Result<uint8_t> current = rdChecked(address);
  if (!current) {
    return Result<void>::Failure(current.error);
  }
  if (detail::ReadShowsUnackedOff(address, current.value, mask, config_.ack)) {
    return fail(Error::Unsafe, "pf1550: a protection reads as off, register not written");
  }
  // Bits the chip changes by itself and the call did not name are not compared.
  const auto own = static_cast<uint8_t>(static_cast<uint32_t>(detail::SelfChangingBits(address)) &
                                        ~static_cast<uint32_t>(mask));
  return rmw(address, current.value, mask, value, own);
}

template <I2cPort Port>
Result<void> PF1550<Port>::rmw(uint8_t address, uint8_t current, uint8_t mask, uint8_t value,
                               uint8_t unchecked) noexcept {
  const auto m = static_cast<uint32_t>(mask);
  const auto next = static_cast<uint8_t>((static_cast<uint32_t>(current) & ~m) |
                                         (static_cast<uint32_t>(value) & m));
  if (const Result<void> w = wr(address, next); !w) {
    return w;
  }
  // Verify: the OTP-loaded bits are "RW1S" and the data sheet is not clear that a written 0 takes
  // (ambiguity C4), so every typed write is read back.
  const Result<uint8_t> back = rd(address);
  if (!back) {
    return Result<void>::Failure(back.error);
  }
  // The whole byte is compared (a bad read that was written back must not pass), apart from the
  // bits the chip changes by itself.
  if (((static_cast<uint32_t>(back.value) ^ static_cast<uint32_t>(next)) &
       ~static_cast<uint32_t>(unchecked)) != 0U) {
    return fail(Error::VerifyFailed, "pf1550: register read back differently");
  }
  const auto changed =
      static_cast<uint8_t>((static_cast<uint32_t>(current) ^ static_cast<uint32_t>(next)) & m &
                           ~static_cast<uint32_t>(unchecked) &
                           ~static_cast<uint32_t>(detail::SelfChangingBits(address)) &
                           ~static_cast<uint32_t>(detail::PartialResetBits(address)));
  if (changed != 0U) {
    recordSentinel(address, changed, next);
  }
  return Result<void>::Success();
}

/// A switch-off is never refused for want of a good read (hardware lessons 12c): the bits other
/// than `mask` come from a validated read when there is one, else they are written as zero, and
/// the write goes out either way. Only the masked bits are read back.
template <I2cPort Port>
Result<void> PF1550<Port>::updateOff(uint8_t address, uint8_t mask, uint8_t value) noexcept {
  const reg::Access access = reg::AccessOf(address);
  if ((access == reg::Access::Status) || (access == reg::Access::Command)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  Result<uint8_t> current = rdChecked(address);
  if (current && detail::ReadShowsUnackedOff(address, current.value, mask, config_.ack)) {
    current = Result<uint8_t>::Failure(Error::Unsafe); // not believed: the other bits are zero
  }
  const uint32_t base = current ? static_cast<uint32_t>(current.value) : 0U;
  if (!current) {
    detail::Log(bus_.GetPort(), LogLevel::Warning, "pf1550: no valid read, switching off anyway");
  }
  const auto m = static_cast<uint32_t>(mask);
  const auto next = static_cast<uint8_t>((base & ~m) | (static_cast<uint32_t>(value) & m));
  if (const Result<void> w = wr(address, next); !w) {
    return w;
  }
  const Result<uint8_t> back = rd(address);
  if (!back) {
    return Result<void>::Failure(back.error);
  }
  if (((static_cast<uint32_t>(back.value) ^ static_cast<uint32_t>(next)) & m) != 0U) {
    return fail(Error::VerifyFailed, "pf1550: register read back differently");
  }
  // The bits are now switched off and read back so: that is what a reset must not undo (a reset
  // brings a rail back to its OTP state, which may be on).
  const auto off_bits =
      static_cast<uint8_t>(m & ~static_cast<uint32_t>(detail::SelfChangingBits(address)) &
                           ~static_cast<uint32_t>(detail::PartialResetBits(address)));
  if (off_bits != 0U) {
    recordSentinel(address, off_bits, next);
  }
  return Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::applyPlan(std::span<const detail::RegWrite> plan) noexcept {
  for (const detail::RegWrite& w : plan) {
    if (w.mask == 0U) {
      continue;
    }
    if (const Result<void> r = update(w.reg, w.mask, w.value); !r) {
      return r;
    }
  }
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Result<void> PF1550<Port>::identify() noexcept {
  // Two agreeing reads: one wrong read must not say that another device answers.
  const Result<uint8_t> id = rdChecked(reg::kDeviceId);
  if (!id) {
    return Result<void>::Failure(id.error);
  }
  if (id.value == detail::kByteZero) {
    // A line stuck low reads 0x00 twice: that is a bus fault, not another device (an all-ones read
    // is refused by `rdChecked`), and it must not take a switch-off away.
    return fail(Error::Bus, "pf1550: DEVICE_ID reads 0x00, a bus line is stuck");
  }
  if (id.value != reg::kDeviceIdValue) {
    return fail(Error::DeviceIdMismatch, "pf1550: DEVICE_ID is not 0x7C");
  }
  return Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (const Error e = Validate(config_); e != Error::Ok) {
    return fail(e, "pf1550: invalid Config");
  }
  if (!bus_.GetPort().EnsureInitialized()) {
    return fail(Error::Bus, "pf1550: the port could not bring the bus up");
  }
  if (const Result<void> r = identify(); !r) {
    // A bus error says nothing about the chip: a driver that was identified stays so, and a
    // switch-off still goes out (hardware lessons 12c). Another device answering clears it.
    if (r.error == Error::DeviceIdMismatch) {
      identified_ = false;
    }
    return r;
  }
  identified_ = true;
  sentinels_.fill(
      detail::Sentinel{}); // what the driver wrote before a reset is not what the chip holds
  intb_complete_ = false;  // the mask registers are unknown until `SetIrqMask()` (INTB covers some)
  if (const Result<void> r = checkDeclaredDvs(); !r) {
    return r;
  }
  Result<ChargerAsFound> found = readChargerAsFound(false);
  const bool unsafe = (found.error == Error::OutOfLimits) || (found.error == Error::Unsafe);
  if (unsafe && (config_.as_found_policy == AsFoundPolicy::MakeSafe)) {
    if (const Result<void> r = makeSafe(); !r) {
      return r;
    }
    found = readChargerAsFound(false);
  }
  if (!found) {
    return fail(found.error, "pf1550: the charger as found is not covered by Limits");
  }
  initialized_ = true;
  detail::Log(bus_.GetPort(), LogLevel::Info, "pf1550: initialised");
  return Result<void>::Success();
}

/// The DVS mode of SW1 and SW2 is an OTP setting the chip cannot report. For a listed option
/// (Table 85) the table decides, and a declaration that contradicts it is refused: a rail written
/// in the wrong mode ends at 3.3 V (Table 31). For any other part the declaration stands, and a
/// declared DVS mode needs `dvs_on_unlisted_part`.
template <I2cPort Port>
Result<void> PF1550<Port>::checkDeclaredDvs() noexcept {
  const Result<uint8_t> flavor = rdChecked(reg::kOtpFlavor);
  if (!flavor) {
    return Result<void>::Failure(flavor.error);
  }
  const Result<StartupProfile> profile = StartupProfileOf(DecodeOption(flavor.value));
  const std::array<bool, 2> declared = {config_.sw1.dvs == Dvs::Enabled,
                                        config_.sw2.dvs == Dvs::Enabled};
  const std::array<bool, 2> applied = {config_.sw1.apply, config_.sw2.apply};
  const std::array<bool, 2> actual = {profile.value.sw1_dvs, profile.value.sw2_dvs};
  for (std::size_t i = 0U; i < declared.size(); ++i) {
    if (!profile.ok()) {
      dvs_[i] = declared[i];
      if (declared[i] && !config_.ack.dvs_on_unlisted_part) {
        return fail(Error::Unsafe, "pf1550: DVS declared on a part Table 85 does not list");
      }
    } else {
      // The table decides how the registers are read. A block the application configures must
      // have been declared in the mode the part has, or its voltages would be written wrongly.
      dvs_[i] = actual[i];
      if (applied[i] && (declared[i] != actual[i])) {
        return fail(Error::Refused, "pf1550: the declared DVS mode differs from Table 85");
      }
    }
  }
  return Result<void>::Success();
}

template <I2cPort Port>
bool PF1550<Port>::dvsOf(Rail rail) const noexcept {
  if (rail == Rail::Sw1) {
    return dvs_[0];
  }
  return (rail == Rail::Sw2) && dvs_[1];
}

template <I2cPort Port>
Result<void> PF1550<Port>::makeSafe() noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Warning,
              "pf1550: charger found unsafe, set to linear only");
  return setMode(ChargerMode::LinearOnly);
}

// -------------------------------------------------------------------------------------------------
// Identity and state
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Result<VariantInfo> PF1550<Port>::ReadVariant() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<VariantInfo>::Failure(r.error);
  }
  const Result<uint8_t> id = rd(reg::kDeviceId);
  const Result<uint8_t> flavor = id ? rdChecked(reg::kOtpFlavor) : id;
  const Result<uint8_t> rev = flavor ? rd(reg::kSiliconRev) : flavor;
  const Result<uint8_t> addr = rev ? rd(reg::kI2cAddr) : rev;
  if (!addr) {
    return Result<VariantInfo>::Failure(addr.error);
  }
  VariantInfo v{};
  v.device_id = id.value;
  v.flavor_raw = flavor.value;
  v.option = DecodeOption(flavor.value);
  v.silicon_rev_raw = rev.value;
  v.i2c_slave_addr_code = static_cast<uint8_t>(reg::I2cSlaveAddr::Extract(addr.value));
  return Result<VariantInfo>::Success(v);
}

template <I2cPort Port>
Result<PowerState> PF1550<Port>::ReadPowerState() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<PowerState>::Failure(r.error);
  }
  const Result<uint8_t> raw = rd(reg::kStateInfo);
  if (!raw) {
    return Result<PowerState>::Failure(raw.error);
  }
  return Result<PowerState>::Success(detail::StateOf(raw.value));
}

template <I2cPort Port>
Result<ChargerAsFound> PF1550<Port>::readChargerAsFound(bool as_if_on) noexcept {
  constexpr std::array<uint8_t, 7> kRegs = {reg::kChgOper,      reg::kChgTmr,     reg::kChgCurrCnfg,
                                            reg::kBattReg,      reg::kBatfetCnfg, reg::kThmRegCnfg,
                                            reg::kVbusInlimCnfg};
  std::array<uint8_t, 7> v{};
  for (std::size_t i = 0U; i < kRegs.size(); ++i) {
    const Result<uint8_t> r = rdChecked(kRegs[i]);
    if (!r) {
      return Result<ChargerAsFound>::Failure(r.error);
    }
    v[i] = r.value;
  }
  ChargerAsFound f{};
  f.chg_oper = v[0];
  f.mode = detail::ModeOf(v[0]);
  f.charge_voltage_mv = ChargeVoltageMv(reg::Chgcv::Extract(v[3]));
  const Result<uint32_t> cc = ChargeCurrentMa(reg::ChgCc::Extract(v[2]));
  const Result<uint32_t> in = InputLimitMa(reg::VbusLinIlim::Extract(v[6]));
  f.charge_current_ma = cc.value;
  f.input_limit_ma = in.value;
  f.fast_timer = static_cast<FastChargeTimer>(reg::Fchgtime::Extract(v[1]));
  f.thermistor = static_cast<Thermistor>(reg::ThmCnfg::Extract(v[5]));
  f.batfet_oc = static_cast<BatfetOc>(reg::BatfetOc::Extract(v[4]));
  f.disbatfet = (static_cast<uint32_t>(v[0]) & reg::kChgOperDisbatfet) != 0U;
  Error verdict = Error::Ok;
  if (as_if_on || (f.mode == ChargerMode::Charge)) {
    // A reserved code reads as 0 mA: not covered by any limit, so it is out of limits too.
    verdict = detail::AsFoundVerdict(f, cc.ok() && in.ok(), config_.limits, config_.ack);
  } else if (detail::Unacknowledged(f.disbatfet, config_.ack.allow_batfet_off)) {
    // The battery path forced off is reported whatever the charger mode is.
    verdict = Error::Unsafe;
  } else {
    // The charger is not on and the battery path is as acknowledged: nothing to report.
  }
  f.safe = (verdict == Error::Ok);
  return Result<ChargerAsFound>{.value = f, .error = verdict};
}

template <I2cPort Port>
Result<ChargerAsFound> PF1550<Port>::CheckChargerAgainstLimits() noexcept {
  if (const Result<void> r = requireIdentified(); !r) {
    return Result<ChargerAsFound>::Failure(r.error);
  }
  return readChargerAsFound(false);
}

template <I2cPort Port>
Result<ChargerStatus> PF1550<Port>::ReadChargerStatus() noexcept {
  const Result<ChargerStatus> s = readChargerStatus();
  if (!s) {
    return s;
  }
  // The status path of a charger without the watchdog: a cable removal that put the OTP input
  // limit back is found and put right here too (ServiceChargerWatchdog() does the same).
  if (const Result<void> limit = checkInputLimit(); !limit) {
    return Result<ChargerStatus>::Failure(limit.error);
  }
  return s;
}

template <I2cPort Port>
Result<ChargerStatus> PF1550<Port>::readChargerStatus() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<ChargerStatus>::Failure(r.error);
  }
  const Result<uint8_t> vbus = rd(reg::kVbusSns);
  const Result<uint8_t> chg = vbus ? rd(reg::kChgSns) : vbus;
  const Result<uint8_t> batt = chg ? rd(reg::kBattSns) : chg;
  const Result<uint8_t> again = batt ? rd(reg::kChgSns) : batt;
  if (!again) {
    return Result<ChargerStatus>::Failure(again.error);
  }
  const ChargerStatus s =
      detail::BuildChargerStatus(vbus.value, chg.value, batt.value, again.value);
  if (detail::IsCharging(s.state)) {
    restarts_ = 0U;
  }
  return Result<ChargerStatus>::Success(s);
}

template <I2cPort Port>
Result<bool> PF1550<Port>::noLatchedFault() noexcept {
  constexpr std::array<uint8_t, 5> kFaultRegs = {reg::kSwIntStat0, reg::kSwIntStat1,
                                                 reg::kLdoIntStat0, reg::kTempIntStat0,
                                                 reg::kMiscIntStat0};
  constexpr std::array<IrqGroup, 5> kGroups = {IrqGroup::SwLowSide, IrqGroup::SwHighSide,
                                               IrqGroup::Ldo, IrqGroup::Temp, IrqGroup::Misc};
  for (std::size_t i = 0U; i < kFaultRegs.size(); ++i) {
    const Result<uint8_t> v = rd(kFaultRegs[i]);
    if (!v) {
      return Result<bool>::Failure(v.error);
    }
    noteStatus(static_cast<std::size_t>(detail::ToUnderlying(kGroups[i])), v.value);
  }
  const Result<ChargerStatus> charger = readChargerStatus();
  if (!charger) {
    return Result<bool>::Failure(charger.error);
  }
  return Result<bool>::Success(!HasFault(latch_) && (charger.value.fault == Fault::None));
}

template <I2cPort Port>
Result<bool> PF1550<Port>::railsAsConfigured() noexcept {
  for (std::size_t i = 0U; i < kRailCount; ++i) {
    const auto rail = static_cast<Rail>(i);
    const RailModes* modes = modesOf(rail);
    if (modes == nullptr) {
      continue;
    }
    const Result<RailReadout> r = ReadRail(rail);
    if (!r) {
      return Result<bool>::Failure(r.error);
    }
    if ((r.value.enabled_run != modes->run) || (r.value.enabled_standby != modes->standby) ||
        (r.value.enabled_sleep != modes->sleep)) {
      return Result<bool>::Success(false);
    }
  }
  return Result<bool>::Success(true);
}

template <I2cPort Port>
Result<SelfCheckReport> PF1550<Port>::SelfCheck() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<SelfCheckReport>::Failure(r.error);
  }
  const Result<uint8_t> id = rd(reg::kDeviceId);
  const Result<PowerState> state = id ? ReadPowerState() : Result<PowerState>::Failure(id.error);
  const Result<bool> rails = state ? railsAsConfigured() : Result<bool>::Failure(state.error);
  const Result<bool> clean = rails ? noLatchedFault() : Result<bool>::Failure(rails.error);
  if (!clean) {
    return Result<SelfCheckReport>::Failure(clean.error);
  }
  const Result<uint8_t> lim = rdChecked(reg::kVbusInlimCnfg);
  if (!lim) {
    return Result<SelfCheckReport>::Failure(lim.error);
  }
  if (const Result<void> r = checkReset(); !r) {
    return Result<SelfCheckReport>::Failure(r.error);
  }
  SelfCheckReport report{};
  report.identity_ok = (id.value == reg::kDeviceIdValue);
  report.state_run = (state.value == PowerState::Run);
  report.rails_as_configured = rails.value;
  report.no_latched_fault = clean.value;
  report.input_limit_ok = inputLimitOk(lim.value);
  return Result<SelfCheckReport>::Success(report);
}

template <I2cPort Port>
Result<std::array<RailReadout, kRailCount>> PF1550<Port>::readRails() noexcept {
  std::array<RailReadout, kRailCount> rails{};
  for (std::size_t i = 0U; i < kRailCount; ++i) {
    const Result<RailReadout> r = ReadRail(static_cast<Rail>(i));
    if (!r) {
      return Result<std::array<RailReadout, kRailCount>>::Failure(r.error);
    }
    rails[i] = r.value;
  }
  return Result<std::array<RailReadout, kRailCount>>::Success(rails);
}

template <I2cPort Port>
Result<Snapshot> PF1550<Port>::ReadSnapshot() noexcept {
  const Result<VariantInfo> variant = ReadVariant();
  const Result<PowerState> state =
      variant ? ReadPowerState() : Result<PowerState>::Failure(variant.error);
  const Result<ChargerStatus> charger =
      state ? readChargerStatus() : Result<ChargerStatus>::Failure(state.error);
  const auto rails =
      charger ? readRails() : Result<std::array<RailReadout, kRailCount>>::Failure(charger.error);
  const Result<PowerDownSequence> down =
      rails ? ReadPowerDownSequence() : Result<PowerDownSequence>::Failure(rails.error);
  const Result<uint8_t> pwr0 = down ? rd(reg::kPwrCtrl0) : Result<uint8_t>::Failure(down.error);
  if (!pwr0) {
    return Result<Snapshot>::Failure(pwr0.error);
  }
  Snapshot s{};
  s.variant = variant.value;
  s.state = state.value;
  s.charger = charger.value;
  s.rails = rails.value;
  s.power_down = down.value;
  s.por_delay_code = static_cast<uint8_t>(reg::PorDly::Extract(pwr0.value));
  for (std::size_t i = 0U; i < kIrqGroups; ++i) {
    const Result<uint8_t> v = rd(kIrqSenseRegs[i]);
    if (!v) {
      return Result<Snapshot>::Failure(v.error);
    }
    s.sense[i] = v.value;
  }
  return Result<Snapshot>::Success(s);
}

// -------------------------------------------------------------------------------------------------
// Charger
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Result<void> PF1550<Port>::setMode(ChargerMode mode) noexcept {
  const auto code = static_cast<uint8_t>(detail::ToUnderlying(mode));
  if (mode != ChargerMode::Charge) {
    return updateOff(reg::kChgOper, reg::ChgOperMode::kMask, code);
  }
  return update(reg::kChgOper, reg::ChgOperMode::kMask, code);
}

template <I2cPort Port>
Result<void> PF1550<Port>::SetChargerMode(ChargerMode mode) noexcept {
  if (const Result<void> r = requireIdentified(); !r) {
    return r;
  }
  if (!detail::IsEnumerator(mode)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (mode != ChargerMode::Charge) {
    // Switching the charger off, or to the linear regulator alone, is never refused once the chip
    // has been identified: an unsafe charger found at start-up must be stoppable.
    return setMode(mode);
  }
  if (const Result<void> r = requireLive(); !r) {
    return r;
  }
  // Charging: the registers the chip holds must be covered by Limits and the acknowledgements, and
  // the charger must report no fault (a timer fault leaves only by off and on, section 8.9).
  const Result<ChargerAsFound> found = readChargerAsFound(true);
  if (!found) {
    // A refused start never leaves a charger running that the limits do not cover.
    const bool refused = (found.error == Error::OutOfLimits) || (found.error == Error::Unsafe);
    if (refused && (found.value.mode == ChargerMode::Charge)) {
      if (const Result<void> off = setMode(ChargerMode::Off); !off) {
        detail::Log(bus_.GetPort(), LogLevel::Error, "pf1550: the charger could not be stopped");
      }
    }
    return Result<void>::Failure(found.error);
  }
  const Result<ChargerStatus> status = readChargerStatus();
  if (!status) {
    return Result<void>::Failure(status.error);
  }
  if (status.value.fault != Fault::None) {
    return fail(Error::ChargerFault, "pf1550: the charger reports a fault, charging not enabled");
  }
  return setMode(ChargerMode::Charge);
}

template <I2cPort Port>
Result<void> PF1550<Port>::RestartCharger() noexcept {
  if (const Result<void> r = requireIdentified(); !r) {
    return r;
  }
  if (restarts_ >= kMaxChargerRestarts) {
    return fail(Error::Refused, "pf1550: too many charger restarts without charging");
  }
  ++restarts_;
  if (const Result<void> r = setMode(ChargerMode::Off); !r) {
    return r;
  }
  return SetChargerMode(ChargerMode::Charge);
}

template <I2cPort Port>
Result<void> PF1550<Port>::ServiceChargerWatchdog() noexcept {
  if (const Result<void> r = requireLive(); !r) {
    return r;
  }
  // The input limit is checked first, so that a BATFET_CNFG read that is refused does not skip it;
  // its error (the charger was switched off) is returned after the service.
  const Result<void> limit = checkInputLimit();
  // The most frequent write of the driver must not carry a bad read into BATFET_CNFG: two reads
  // agree, and a read that shows the battery overcurrent protection off is believed only when the
  // application accepted that (a stuck line reads 0x00).
  const Result<uint8_t> cur = rdChecked(reg::kBatfetCnfg);
  if (!cur) {
    return Result<void>::Failure(cur.error);
  }
  if ((reg::BatfetOc::Extract(cur.value) == 0U) && !config_.ack.allow_batfet_oc_off) {
    return fail(Error::Unsafe, "pf1550: BATFET_OC reads as disabled, watchdog not serviced");
  }
  // WDTCLR = 01 clears the timer (Table 161). Whether the two bits hold what is written, and
  // whether a clear needs a 0 to 01 edge, is not stated (C58): a held 01 is written to 00 first,
  // so a service is an edge either way; the bits are excluded from the comparison.
  using detail::operator""_u8;
  using detail::operator""_u32;
  if (reg::Wdtclr::Extract(cur.value) != 0U) {
    if (const Result<void> r = wr(reg::kBatfetCnfg, reg::Wdtclr::Insert(cur.value, 0_u32)); !r) {
      return r;
    }
  }
  if (const Result<void> r =
          rmw(reg::kBatfetCnfg, cur.value, reg::Wdtclr::kMask, 1_u8, reg::Wdtclr::kMask);
      !r) {
    return r;
  }
  return limit;
}

template <I2cPort Port>
bool PF1550<Port>::inputLimitOk(uint8_t raw) const noexcept {
  const uint32_t code = reg::VbusLinIlim::Extract(raw);
  const Result<uint32_t> ma = InputLimitMa(code);
  if (!ma.ok() || (ma.value > config_.limits.input_max_ma)) {
    return false;
  }
  return !config_.charger.apply || (ma.value == config_.charger.input_limit_ma);
}

template <I2cPort Port>
Result<void> PF1550<Port>::checkInputLimit() noexcept {
  const Result<uint8_t> oper = rdChecked(reg::kChgOper);
  if (!oper) {
    return Result<void>::Failure(oper.error);
  }
  // Charge is covered by Limits. The linear regulator alone draws from the same input, so a limit
  // the application configured is kept there too; without a configured limit the as-found check
  // of start-up (which accepts the part's own LinearOnly state) is the rule and nothing is judged.
  const ChargerMode mode = detail::ModeOf(oper.value);
  if ((mode == ChargerMode::Off) || ((mode == ChargerMode::LinearOnly) && !config_.charger.apply)) {
    return Result<void>::Success();
  }
  const Result<uint8_t> lim = rdChecked(reg::kVbusInlimCnfg);
  if (!lim) {
    return Result<void>::Failure(lim.error);
  }
  if (inputLimitOk(lim.value)) {
    return Result<void>::Success();
  }
  if (config_.charger.apply) {
    // A cable removal put the OTP limit back (Table 163). While VBUSIN is invalid the field is held
    // in reset and a write would not hold: it is left, and written again once the supply is valid.
    const Result<VbusFlags> vbus = readVbus();
    if (!vbus) {
      return Result<void>::Failure(vbus.error);
    }
    if (!vbus.value.valid) {
      return Result<void>::Success();
    }
    // A chip that reset meanwhile holds its own values: nothing is written over them (the status
    // and interrupt paths do not look for a reset otherwise).
    if (const Result<void> live = checkReset(); !live) {
      return live;
    }
    detail::Log(bus_.GetPort(), LogLevel::Warning,
                "pf1550: the input limit was reverted, restored");
    const uint8_t code = InputLimitCode(config_.charger.input_limit_ma).value;
    return update(reg::kVbusInlimCnfg, reg::VbusLinIlim::kMask, static_cast<uint8_t>(code << 3U));
  }
  if (const Result<void> off = setMode(ChargerMode::Off); !off) {
    return off;
  }
  return fail(Error::OutOfLimits, "pf1550: the input limit is above Limits, charger switched off");
}

template <I2cPort Port>
Result<void> PF1550<Port>::applyCharger() noexcept {
  const ChargerConfig& c = config_.charger;
  if (!c.apply) {
    return Result<void>::Success();
  }
  // Charging stops while the parameters change; CHG_OPER's mode is written last (C16, C49).
  const ChargerMode park =
      (c.mode == ChargerMode::Off) ? ChargerMode::Off : ChargerMode::LinearOnly;
  if (const Result<void> r = setMode(park); !r) {
    return r;
  }
  const auto plan = detail::ChargerPlan(c);
  if (const Result<void> r = applyPlan(std::span<const detail::RegWrite>{plan}); !r) {
    return r;
  }
  const auto tail = detail::ChargerTailPlan(c);
  if (const Result<void> r = applyPlan(std::span<const detail::RegWrite>{tail}); !r) {
    return r;
  }
  return (c.mode == ChargerMode::Charge) ? SetChargerMode(ChargerMode::Charge)
                                         : Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Rails
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
const BuckConfig* PF1550<Port>::buckConfig(Rail rail) const noexcept {
  switch (rail) {
  case Rail::Sw1:
    return &config_.sw1;
  case Rail::Sw2:
    return &config_.sw2;
  case Rail::Sw3:
    return &config_.sw3;
  case Rail::Ldo1:
  case Rail::Ldo2:
  case Rail::Ldo3:
  case Rail::VrefDdr:
  default:
    break;
  }
  return nullptr;
}

template <I2cPort Port>
const RailModes* PF1550<Port>::modesOf(Rail rail) const noexcept {
  switch (rail) {
  case Rail::Sw1:
    return config_.sw1.apply ? &config_.sw1.modes : nullptr;
  case Rail::Sw2:
    return config_.sw2.apply ? &config_.sw2.modes : nullptr;
  case Rail::Sw3:
    return config_.sw3.apply ? &config_.sw3.modes : nullptr;
  case Rail::Ldo1:
    return config_.ldo1.apply ? &config_.ldo1.modes : nullptr;
  case Rail::Ldo2:
    return config_.ldo2.apply ? &config_.ldo2.modes : nullptr;
  case Rail::Ldo3:
    return config_.ldo3.apply ? &config_.ldo3.modes : nullptr;
  case Rail::VrefDdr:
    return config_.vrefddr.apply ? &config_.vrefddr.modes : nullptr;
  default:
    break;
  }
  return nullptr;
}

template <I2cPort Port>
Result<RailReadout> PF1550<Port>::ReadRail(Rail rail) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<RailReadout>::Failure(r.error);
  }
  if (!detail::IsEnumerator(rail)) {
    return Result<RailReadout>::Failure(Error::InvalidArgument);
  }
  const detail::RailRegs& regs = detail::RegsOf(rail);
  const Result<uint8_t> ctrl = rd(regs.ctrl);
  const Result<uint8_t> volt = (ctrl && (regs.volt != 0U)) ? rd(regs.volt) : ctrl;
  if (!volt) {
    return Result<RailReadout>::Failure(volt.error);
  }
  RailReadout out{};
  const auto bits = static_cast<uint32_t>(ctrl.value);
  out.enabled_run = (bits & reg::kCtrlEn) != 0U;
  out.enabled_standby = (bits & reg::kCtrlStbyEn) != 0U;
  out.enabled_sleep = (bits & reg::kCtrlOmode) != 0U;
  out.low_power = (bits & reg::kCtrlLpwr) != 0U;
  out.load_switch =
      ((rail == Rail::Ldo1) || (rail == Rail::Ldo3)) && ((bits & reg::kLdoLsEn) != 0U);
  out.volt.value = detail::VoltUv(rail, volt.value, dvsOf(rail));
  return Result<RailReadout>::Success(out);
}

/// A rail must not be enabled at a voltage its consumer does not tolerate, and the voltage a
/// register holds after a reset or from a previous owner is not known: the registers that the
/// enable will bring into effect and the call does not write are read and checked against Limits.
template <I2cPort Port>
Result<void> PF1550<Port>::checkHeldVoltage(Rail rail, const RailModes& modes, bool run_written,
                                            bool standby_written, bool sleep_written) noexcept {
  struct Item {
    uint8_t address;
    bool wanted;
    bool written;
  };
  const detail::RailRegs& regs = detail::RegsOf(rail);
  const RailLimits& lim = detail::LimitsOf(config_.limits, rail);
  // The linear regulators, SW3 and a non-DVS buck have one voltage for every state.
  const bool single = (regs.stby_volt == 0U) || !dvsOf(rail);
  const std::array<Item, 3> items = {{
      {regs.volt, modes.run || (single && (modes.standby || modes.sleep)), run_written},
      {regs.stby_volt, !single && modes.standby, standby_written},
      {regs.slp_volt, !single && modes.sleep, sleep_written},
  }};
  for (const Item& item : items) {
    if (!item.wanted || item.written || (item.address == 0U)) {
      continue;
    }
    const Result<uint8_t> v = rdChecked(item.address);
    if (!v) {
      return Result<void>::Failure(v.error);
    }
    const uint32_t mv = detail::VoltUv(rail, v.value, dvsOf(rail)) / 1000U;
    if ((lim.max_mv == 0U) || (mv < lim.min_mv) || (mv > lim.max_mv)) {
      return fail(Error::OutOfLimits, "pf1550: the rail's held voltage is outside Limits");
    }
  }
  return Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::SetRailModes(Rail rail, const RailModes& modes) noexcept {
  // A request that switches the rail off in every state needs only an identified chip and is
  // written even when the register cannot be read (hardware lessons 12c).
  const bool off = !modes.run && !modes.standby && !modes.sleep;
  if (const Result<void> r = off ? requireIdentified() : requireInitialized(); !r) {
    return r;
  }
  if (!detail::IsEnumerator(rail)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (off) {
    return updateOff(detail::RegsOf(rail).ctrl, detail::kModeMask, detail::ModeBits(modes));
  }
  // The checks that need no chip data come first: a bad request sends nothing.
  if (const Error e =
          detail::ValidateModes(modes, detail::LimitsOf(config_.limits, rail), config_.ack);
      e != Error::Ok) {
    return fail(e, "pf1550: rail modes rejected");
  }
  if (const Result<void> r = checkReset(); !r) {
    return r;
  }
  if (const Result<void> r = checkHeldVoltage(rail, modes, false, false, false); !r) {
    return r;
  }
  return update(detail::RegsOf(rail).ctrl, detail::kModeMask, detail::ModeBits(modes));
}

template <I2cPort Port>
Result<void> PF1550<Port>::clearDvsDone(Rail rail) noexcept {
  const auto bit =
      static_cast<uint8_t>(1U << (static_cast<uint32_t>(detail::ToUnderlying(rail)) & 1U));
  return wr(reg::kSwIntStat2, bit);
}

template <I2cPort Port>
Result<void> PF1550<Port>::waitDvsDone(Rail rail) noexcept {
  const uint32_t bit = 1U << (static_cast<uint32_t>(detail::ToUnderlying(rail)) & 1U);
  for (uint32_t waited = 0U; waited < kDvsWaitMaxUs; waited += kDvsPollUs) {
    detail::DelayUs(bus_.GetPort(), kDvsPollUs);
    const Result<uint8_t> v = rd(reg::kSwIntStat2);
    if (!v) {
      return Result<void>::Failure(v.error);
    }
    // The other rail's DVS-done bit is cleared with ours when it is set: keep it for the caller.
    noteStatus(static_cast<std::size_t>(detail::ToUnderlying(IrqGroup::SwDvs)),
               static_cast<uint8_t>(static_cast<uint32_t>(v.value) & ~bit));
    if ((static_cast<uint32_t>(v.value) & bit) != 0U) {
      return clearDvsDone(rail);
    }
  }
  return fail(Error::Timeout, "pf1550: the DVS step did not finish in time");
}

/// A DVS ramp, and so a DVS-done event, follows only a changed run voltage of a rail that is on.
template <I2cPort Port>
Result<bool> PF1550<Port>::needsDvsWait(Rail rail, Microvolts uv) noexcept {
  const detail::RailRegs& regs = detail::RegsOf(rail);
  const Result<uint8_t> volt = rd(regs.volt);
  const Result<uint8_t> ctrl = volt ? rd(regs.ctrl) : volt;
  if (!ctrl) {
    return Result<bool>::Failure(ctrl.error);
  }
  const uint32_t code = BuckDvsCode(uv.value).value;
  const bool changed = reg::BuckVolt6::Extract(volt.value) != code;
  const bool on = (static_cast<uint32_t>(ctrl.value) & reg::kCtrlEn) != 0U;
  return Result<bool>::Success(changed && on);
}

template <I2cPort Port>
Result<void> PF1550<Port>::writeVoltage(Rail rail, Microvolts uv) noexcept {
  const detail::RailRegs& regs = detail::RegsOf(rail);
  if ((rail != Rail::Sw1) && (rail != Rail::Sw2)) {
    const uint8_t mask = (rail == Rail::Ldo2) ? reg::Ldo4Volt::kMask : reg::Ldo5Volt::kMask;
    return update(regs.volt, mask, LdoCode(rail, uv.value).value);
  }
  const Result<bool> wait = needsDvsWait(rail, uv);
  if (!wait) {
    return Result<void>::Failure(wait.error);
  }
  if (wait.value) {
    if (const Result<void> r = clearDvsDone(rail); !r) {
      return r;
    }
  }
  if (const Result<void> r = update(regs.volt, reg::BuckVolt6::kMask, BuckDvsCode(uv.value).value);
      !r) {
    return r;
  }
  return wait.value ? waitDvsDone(rail) : Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::SetRailVoltage(Rail rail, Microvolts uv) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return r;
  }
  if (!detail::IsEnumerator(rail) || (rail == Rail::VrefDdr) || (uv.value == 0U)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const BuckConfig* buck = buckConfig(rail);
  if ((buck != nullptr) && ((rail == Rail::Sw3) || !dvsOf(rail))) {
    return fail(Error::ReadOnlyRail, "pf1550: this buck's voltage is read only");
  }
  if (const Error e = detail::ValidateVolt(rail, uv, config_.limits); e != Error::Ok) {
    return fail(e, "pf1550: voltage rejected");
  }
  if (const Result<void> r = checkReset(); !r) {
    return r;
  }
  return writeVoltage(rail, uv);
}

template <I2cPort Port>
Result<void> PF1550<Port>::EnableLoadSwitch(Rail rail, LoadSwitchConfirm confirm) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return r;
  }
  if ((rail != Rail::Ldo1) && (rail != Rail::Ldo3)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const RailLimits& lim = detail::LimitsOf(config_.limits, rail);
  if (lim.max_mv == 0U) {
    return fail(Error::OutOfLimits, "pf1550: the rail has no limits window");
  }
  // In load switch mode the output follows the input (Table 55): the input voltage the caller
  // states must be one the consumer tolerates. The switch cannot be turned off again (a reset).
  if ((confirm.input_mv < lim.min_mv) || (confirm.input_mv > lim.max_mv)) {
    return fail(Error::OutOfLimits, "pf1550: the load switch input voltage is outside Limits");
  }
  if (const Result<void> r = checkReset(); !r) {
    return r;
  }
  const Result<uint8_t> ctrl = rdChecked(detail::RegsOf(rail).ctrl);
  if (!ctrl) {
    return Result<void>::Failure(ctrl.error);
  }
  if ((static_cast<uint32_t>(ctrl.value) & reg::kCtrlLpwr) != 0U) {
    return fail(Error::Refused, "pf1550: the rail is in low-power mode, no load switch");
  }
  return update(detail::RegsOf(rail).ctrl, reg::kLdoLsEn, reg::kLdoLsEn);
}

template <I2cPort Port>
Result<void> PF1550<Port>::applyBuck(Rail rail, const BuckConfig& b) noexcept {
  if (!b.apply) {
    return Result<void>::Success();
  }
  const bool volts = dvsOf(rail) && (rail != Rail::Sw3);
  if (const Result<void> r =
          checkHeldVoltage(rail, b.modes, volts && (b.run_uv.value != 0U),
                           volts && (b.standby_uv.value != 0U), volts && (b.sleep_uv.value != 0U));
      !r) {
    return r;
  }
  Result<bool> wait = Result<bool>::Success(false);
  if (volts && (b.run_uv.value != 0U)) {
    wait = needsDvsWait(rail, b.run_uv);
  }
  if (!wait) {
    return Result<void>::Failure(wait.error);
  }
  if (wait.value) {
    if (const Result<void> r = clearDvsDone(rail); !r) {
      return r;
    }
  }
  const auto plan = detail::BuckPlan(rail, b);
  if (const Result<void> r = applyPlan(std::span<const detail::RegWrite>{plan}); !r) {
    return r;
  }
  return wait.value ? waitDvsDone(rail) : Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::applyLdo(Rail rail, const LdoConfig& l) noexcept {
  if (!l.apply) {
    return Result<void>::Success();
  }
  if (const Result<void> r = checkHeldVoltage(rail, l.modes, l.volt.value != 0U, false, false);
      !r) {
    return r;
  }
  const auto plan = detail::LdoPlan(rail, l);
  return applyPlan(std::span<const detail::RegWrite>{plan});
}

template <I2cPort Port>
Result<void> PF1550<Port>::applyRail(Rail rail) noexcept {
  switch (rail) {
  case Rail::Sw1:
    return applyBuck(rail, config_.sw1);
  case Rail::Sw2:
    return applyBuck(rail, config_.sw2);
  case Rail::Sw3:
    return applyBuck(rail, config_.sw3);
  case Rail::Ldo1:
    return applyLdo(rail, config_.ldo1);
  case Rail::Ldo2:
    return applyLdo(rail, config_.ldo2);
  case Rail::Ldo3:
    return applyLdo(rail, config_.ldo3);
  case Rail::VrefDdr: {
    if (!config_.vrefddr.apply) {
      return Result<void>::Success();
    }
    const std::array<detail::RegWrite, 1> plan = {detail::VrefDdrPlan(config_.vrefddr)};
    return applyPlan(std::span<const detail::RegWrite>{plan});
  }
  default:
    break;
  }
  return Result<void>::Failure(Error::InvalidArgument);
}

// -------------------------------------------------------------------------------------------------
// Configure
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Result<void> PF1550<Port>::applyBlocks() noexcept {
  for (const Rail rail : config_.rail_order) {
    if (const Result<void> r = applyRail(rail); !r) {
      return r;
    }
  }
  const auto small = detail::PeripheralPlan(config_);
  if (const Result<void> r = applyPlan(std::span<const detail::RegWrite>{small}); !r) {
    return r;
  }
  const auto power = detail::PowerPlan(config_.power);
  if (const Result<void> r = applyPlan(std::span<const detail::RegWrite>{power}); !r) {
    return r;
  }
  if (config_.power_down.apply) {
    const auto down = detail::PowerDownPlan(config_.power_down.sequence);
    if (const Result<void> r = applyPlan(std::span<const detail::RegWrite>{down}); !r) {
      return r;
    }
  }
  return config_.apply_irq_mask ? SetIrqMask(config_.irq_mask) : Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::Configure() noexcept {
  if (const Result<void> r = requireLive(); !r) {
    return r;
  }
  if (const Error e = Validate(config_); e != Error::Ok) {
    return fail(e, "pf1550: invalid Config");
  }
  if (const Result<void> r = applyBlocks(); !r) {
    return r;
  }
  // A reset in the middle of the configuration must not let the charger block go onto a chip that
  // is back at its start-up state: look once more (the registers written so far are the evidence).
  if (const Result<void> r = checkReset(); !r) {
    return r;
  }
  return applyCharger();
}

// -------------------------------------------------------------------------------------------------
// Power states and the sequencer
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Result<void> PF1550<Port>::RequestStandby(bool standby) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return r;
  }
  // The level is worked out before the port check so the parameter is used in every instantiation
  // (Rule 0.2.2).
  const GpioSignal level = standby ? GpioSignal::ACTIVE : GpioSignal::INACTIVE;
  if constexpr (detail::HasGpioSet<Port>) {
    return bus_.GetPort().GpioSet(CtrlPin::Standby, level) ? Result<void>::Success()
                                                           : Result<void>::Failure(Error::Bus);
  } else {
    static_cast<void>(level);
    return Result<void>::Failure(Error::Unsupported);
  }
}

template <I2cPort Port>
Result<bool> PF1550<Port>::IntbAsserted() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<bool>::Failure(r.error);
  }
  if constexpr (detail::HasGpioGet<Port>) {
    bool asserted = false;
    if (!bus_.GetPort().GpioGet(CtrlPin::Intb, asserted)) {
      return Result<bool>::Failure(Error::Bus);
    }
    return Result<bool>::Success(asserted);
  } else {
    return Result<bool>::Failure(Error::Unsupported);
  }
}

template <I2cPort Port>
Result<VbusFlags> PF1550<Port>::readVbus() noexcept {
  // Only the shutdown calls read this: two agreeing reads, a flipped one must not open the gate.
  const Result<uint8_t> raw = rdChecked(reg::kVbusSns);
  if (!raw) {
    return Result<VbusFlags>::Failure(raw.error);
  }
  return Result<VbusFlags>::Success(detail::DecodeVbus(raw.value));
}

template <I2cPort Port>
Result<void> PF1550<Port>::requireSystemOn() noexcept {
  const Result<uint8_t> raw = rdChecked(reg::kStateInfo);
  if (!raw) {
    return Result<void>::Failure(raw.error);
  }
  const PowerState state = detail::StateOf(raw.value);
  const bool on =
      (state == PowerState::Run) || (state == PowerState::Standby) || (state == PowerState::Sleep);
  return on ? Result<void>::Success() : fail(Error::Refused, "pf1550: the system is not on");
}

template <I2cPort Port>
Result<void> PF1550<Port>::setPwrCtrl3(uint8_t bit) noexcept {
  const Result<uint8_t> cur = rdChecked(reg::kPwrCtrl3);
  if (!cur) {
    return Result<void>::Failure(cur.error);
  }
  // Not read back: the chip powers down. Both command bits are cleared from the value read first,
  // so a corrupted read can never carry the other command into the write (a SHIP request must not
  // turn into CORE_OFF, nor the reverse).
  constexpr uint32_t kCommands = static_cast<uint32_t>(reg::kGotoShip) | reg::kGotoCoreOff;
  const uint32_t kept = static_cast<uint32_t>(cur.value) & ~kCommands;
  return wr(reg::kPwrCtrl3, static_cast<uint8_t>(kept | static_cast<uint32_t>(bit)));
}

// The confirmation is the tag type; its value carries nothing, so the parameter is unnamed.
template <I2cPort Port>
Result<void> PF1550<Port>::EnterShip(ShipConfirm /*confirm*/) noexcept {
  if (const Result<void> r = requireLive(); !r) {
    return r;
  }
  if (const Result<void> r = requireSystemOn(); !r) {
    return r;
  }
  const Result<VbusFlags> vbus = readVbus();
  if (!vbus) {
    return Result<void>::Failure(vbus.error);
  }
  if (vbus.value.valid || !vbus.value.uvlo) {
    return fail(Error::Refused, "pf1550: VBUS is present, SHIP would not be entered");
  }
  return setPwrCtrl3(reg::kGotoShip);
}

// The confirmation is the tag type; its value carries nothing, so the parameter is unnamed.
template <I2cPort Port>
Result<void> PF1550<Port>::EnterCoreOff(CoreOffConfirm /*confirm*/) noexcept {
  if (const Result<void> r = requireLive(); !r) {
    return r;
  }
  if (const Result<void> r = requireSystemOn(); !r) {
    return r;
  }
  const Result<VbusFlags> vbus = readVbus();
  const Result<uint8_t> onkey =
      vbus ? rdChecked(reg::kOnkeyIntSense0) : Result<uint8_t>::Failure(vbus.error);
  if (!onkey) {
    return Result<void>::Failure(onkey.error);
  }
  // ONKEY_PUSHS = 1 means the button is pushed (ONKEY low); Table 83 I needs ONKEY high.
  const bool pushed = (static_cast<uint32_t>(onkey.value) & 1U) != 0U;
  if (vbus.value.valid || pushed) {
    return fail(Error::Refused,
                "pf1550: VBUS valid or ONKEY pushed, CORE_OFF would not be entered");
  }
  return setPwrCtrl3(reg::kGotoCoreOff);
}

template <I2cPort Port>
Result<void> PF1550<Port>::CancelShutdownRequest() noexcept {
  if (const Result<void> r = requireIdentified(); !r) {
    return r;
  }
  using detail::operator""_u8;
  constexpr uint32_t kCommands = static_cast<uint32_t>(reg::kGotoShip) | reg::kGotoCoreOff;
  const Result<uint8_t> cur = rdChecked(reg::kPwrCtrl3);
  if (!cur) {
    return Result<void>::Failure(cur.error);
  }
  // Both command bits cleared, the rest kept, and the whole byte read back.
  return rmw(reg::kPwrCtrl3, cur.value, static_cast<uint8_t>(kCommands), 0_u8, 0_u8);
}

template <I2cPort Port>
Result<PowerDownSequence> PF1550<Port>::ReadPowerDownSequence() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<PowerDownSequence>::Failure(r.error);
  }
  PowerDownSequence s{};
  for (std::size_t i = 0U; i < kRailCount; ++i) {
    const Result<uint8_t> v = rd(detail::kPwrdnSeqRegs[i]);
    if (!v) {
      return Result<PowerDownSequence>::Failure(v.error);
    }
    s.order[i] = static_cast<uint8_t>(reg::PwrdnSeq::Extract(v.value));
  }
  return Result<PowerDownSequence>::Success(s);
}

template <I2cPort Port>
Result<void> PF1550<Port>::SetPowerDownSequence(const PowerDownSequence& sequence) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return r;
  }
  const PowerDownConfig request{.apply = true, .sequence = sequence};
  const Error verdict = detail::ValidatePowerDown(request, config_.ack);
  if (verdict == Error::InvalidArgument) {
    return Result<void>::Failure(verdict);
  }
  if (verdict != Error::Ok) {
    return fail(verdict, "pf1550: the power-down order needs power_down_order_reviewed");
  }
  if (const Result<void> r = checkReset(); !r) {
    return r;
  }
  const auto plan = detail::PowerDownPlan(sequence);
  return applyPlan(std::span<const detail::RegWrite>{plan});
}

// -------------------------------------------------------------------------------------------------
// Interrupts
// -------------------------------------------------------------------------------------------------

template <I2cPort Port>
Result<void> PF1550<Port>::SetIrqMask(const IrqMask& mask) noexcept {
  if (const Result<void> r = requireLive(); !r) {
    return r;
  }
  uint32_t hidden = 0U;
  for (std::size_t i = 0U; i < kIrqGroups; ++i) {
    const uint8_t defined = kIrqDefinedBits[i];
    const auto value = static_cast<uint8_t>(static_cast<uint32_t>(mask.masked[i]) &
                                            static_cast<uint32_t>(defined));
    if (const Result<void> r = update(kIrqMaskRegs[i], defined, value); !r) {
      intb_complete_ = false;
      return r;
    }
    hidden |= value;
  }
  // INTB high proves that nothing is pending only when no source is masked.
  intb_complete_ = (hidden == 0U);
  return Result<void>::Success();
}

template <I2cPort Port>
Result<Categories> PF1550<Port>::ReadCategories() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<Categories>::Failure(r.error);
  }
  const Result<uint8_t> raw = rd(reg::kIntCategory);
  if (!raw) {
    return Result<Categories>::Failure(raw.error);
  }
  const auto bits = static_cast<uint32_t>(raw.value);
  Categories c{};
  c.charger = (bits & 0x01U) != 0U;
  c.sw1 = (bits & 0x02U) != 0U;
  c.sw2 = (bits & 0x04U) != 0U;
  c.sw3 = (bits & 0x08U) != 0U;
  c.ldo = (bits & 0x10U) != 0U;
  c.onkey = (bits & 0x20U) != 0U;
  c.temp = (bits & 0x40U) != 0U;
  c.misc = (bits & 0x80U) != 0U;
  return Result<Categories>::Success(c);
}

template <I2cPort Port>
Result<uint8_t> PF1550<Port>::ReadRegister(uint8_t address) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<uint8_t>::Failure(r.error);
  }
  if (!reg::IsDocumented(address)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  const Result<uint8_t> v = rd(address);
  if (v) {
    // A read of a status register may clear its bits (Tables 90 to 110): the driver keeps them.
    for (std::size_t g = 0U; g < kIrqGroups; ++g) {
      if (kIrqStatRegs[g] == address) {
        noteStatus(g, v.value);
      }
    }
  }
  return v;
}

template <I2cPort Port>
Result<uint8_t> PF1550<Port>::ReadSense(IrqGroup group) noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<uint8_t>::Failure(r.error);
  }
  if (!detail::IsEnumerator(group)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return rd(kIrqSenseRegs[static_cast<std::size_t>(detail::ToUnderlying(group))]);
}

template <I2cPort Port>
Result<void> PF1550<Port>::collectStatus(InterruptEvents& events, uint8_t categories) noexcept {
  for (std::size_t g = 0U; g < kIrqGroups; ++g) {
    if ((static_cast<uint32_t>(categories) &
         static_cast<uint32_t>(CategoryBitsOf(static_cast<IrqGroup>(g)))) == 0U) {
      continue;
    }
    const Result<uint8_t> v = rd(kIrqStatRegs[g]);
    if (!v) {
      return Result<void>::Failure(v.error);
    }
    if (v.value == 0U) {
      continue;
    }
    noteStatus(g, v.value); // a charger byte makes the input limit check owed (see there)
    // Copy out before the clear (ambiguity C3), then write the bits back to clear exactly them.
    events.status[g] = static_cast<uint8_t>(static_cast<uint32_t>(events.status[g]) |
                                            static_cast<uint32_t>(v.value));
    if (const Result<void> w = wr(kIrqStatRegs[g], v.value); !w) {
      return w;
    }
  }
  return Result<void>::Success();
}

template <I2cPort Port>
bool PF1550<Port>::latchEmpty() const noexcept {
  uint32_t any = 0U;
  for (std::size_t i = 0U; i < kIrqGroups; ++i) {
    any |= static_cast<uint32_t>(latch_[i]);
  }
  return any == 0U;
}

template <I2cPort Port>
bool PF1550<Port>::intbIdle() noexcept {
  if constexpr (detail::HasGpioGet<Port>) {
    bool asserted = true;
    return bus_.GetPort().GpioGet(CtrlPin::Intb, asserted) && !asserted && intb_complete_ &&
           latchEmpty() && !limit_pending_;
  } else {
    return false;
  }
}

template <I2cPort Port>
Result<void> PF1550<Port>::collectPasses(InterruptEvents& events) noexcept {
  constexpr uint8_t kMaxPasses = 3U;
  for (uint8_t pass = 0U; pass < kMaxPasses; ++pass) {
    const Result<uint8_t> cat = rd(reg::kIntCategory);
    if (!cat) {
      return Result<void>::Failure(cat.error);
    }
    if (pass == 0U) {
      events.categories = cat.value;
    }
    if (cat.value == 0U) {
      break;
    }
    if (const Result<void> r = collectStatus(events, cat.value); !r) {
      return r;
    }
    events.passes = static_cast<uint8_t>(pass + 1U);
  }
  return Result<void>::Success();
}

template <I2cPort Port>
Result<void> PF1550<Port>::runPendingLimitCheck() noexcept {
  // A charger interrupt (a cable removed or fitted among them): the input limit is checked, so a
  // charger without the watchdog also finds a limit that went back to its OTP value. Reading a
  // charger status byte made the check owed (`noteStatus()`); it stays pending until it has run
  // through, so a failed one is made again by the next call although that call sees no charger
  // interrupt.
  if (!limit_pending_) {
    return Result<void>::Success();
  }
  if (const Result<void> limit = checkInputLimit(); !limit) {
    return limit;
  }
  limit_pending_ = false;
  return Result<void>::Success();
}

template <I2cPort Port>
Result<InterruptEvents> PF1550<Port>::ServiceInterrupts() noexcept {
  if (const Result<void> r = requireInitialized(); !r) {
    return Result<InterruptEvents>::Failure(r.error);
  }
  InterruptEvents events{};
  if (intbIdle()) {
    return Result<InterruptEvents>::Success(events);
  }
  if (const Result<void> r = collectPasses(events); !r) {
    return Result<InterruptEvents>::Failure(r.error);
  }
  if (const Result<void> r = runPendingLimitCheck(); !r) {
    return Result<InterruptEvents>::Failure(r.error);
  }
  // Bits that other calls read (and so cleared in the chip) are reported here as well.
  for (std::size_t g = 0U; g < kIrqGroups; ++g) {
    events.status[g] = static_cast<uint8_t>(static_cast<uint32_t>(events.status[g]) |
                                            static_cast<uint32_t>(latch_[g]));
  }
  events.fault_class = HasFault(events.status);
  latch_.fill(detail::kByteZero);
  return Result<InterruptEvents>::Success(events);
}

} // namespace pf1550

#endif // PF1550_IPP
