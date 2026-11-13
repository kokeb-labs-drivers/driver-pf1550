/**
 * @file fuzz_pf1550.cpp
 * @brief Fuzz target: the driver over a bus whose behaviour the fuzzer controls.
 *
 * What the driver reads comes from the chip over the bus, so read data is the untrusted input. The
 * fuzzer's bytes choose a start-up option and a configuration, then a sequence of driver calls; on
 * every transfer they also choose what the bus does: pass it to the chip model, fail it, flip bits
 * of the data read or replace it. They can also change the world around the chip: a supply, an
 * ONKEY press, a fault, a register poked to any value, an interrupt raised.
 *
 * After every call the target checks the driver's promises (docs/contract.md), and aborts, naming
 * the line, when one breaks:
 *
 * - a failed call returns an `Error` the enum defines;
 * - before `EnsureInitialized()` succeeds, no call makes a transfer: every call returns
 *   `NotInitialized`;
 * - the driver never writes a read-only, key-protected or undocumented register;
 * - no charge voltage or current written exceeds the `Limits`;
 * - no write newly switches off a protection the application did not acknowledge: battery
 *   overcurrent protection (BATFET_OC = 0), the fast-charge timer, the thermistor, the battery
 *   FET (DISBATFET); the reset paths and the undervoltage threshold are not changed without their
 *   acknowledgements; a write never carries both shutdown commands (a bad read must not turn
 *   SHIP into CORE_OFF);
 * - a line can stick (one of the four protection registers reads a fixed value) and free itself;
 *   a stuck line never lets a write carry a protection off that the application did not accept;
 * - a rail enable newly written finds the rail's held voltage inside `Limits`;
 *   the load switch bit is set only during a call to `EnableLoadSwitch()`;
 * - after `DeviceReset` the driver is uninitialised;
 * - SHIP and CORE_OFF are entered only after the matching call returned `Ok`;
 * - after a clean limit-checking call that returned `Ok` (watchdog, charger status, an interrupt
 *   service that saw a charger event or owed a check) the input limit holds (`CheckAfter()`);
 *   a charger byte in the latch owes the check until a service has passed it;
 * - a call that returns `DeviceReset` wrote no input limit;
 * - a sentinel bit that reads differently is seen by `SelfCheck()` (`CheckResetSeen()`);
 * - `ClearLatchedEvents()` makes no transfer and empties the latch;
 * - every log line is a non-empty plain-text message.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang, `-DPF1550_BUILD_FUZZERS=ON`),
 * and as a plain executable that replays the corpus in tests/fuzz/corpus/ as a CTest on any
 * compiler.
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mock_pf1550.hpp" // the chip model, then the bus mock in mock_pf1550_bus.hpp
#include "mock_pf1550_bus.hpp"
#include "pf1550.hpp"
#include "pf1550_codec.hpp"
#include "pf1550_registers.hpp"
#include "test_support.hpp"

namespace {

using pf1550::Error;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    (void)std::fprintf(stderr, "fuzz_pf1550: promise broken at line %d\n", line);
    std::abort();
  }
}

/// What the bus does with one transfer, chosen by the fuzzer; any other value passes it.
constexpr uint8_t kFail = 0xFF;    ///< The transfer fails; it never reached the chip.
constexpr uint8_t kFlip = 0xFE;    ///< Data read: bits flipped (noise). A write: lost.
constexpr uint8_t kReplace = 0xFD; ///< The data read is replaced (another device answered).
constexpr uint8_t kStuck = 0xFC;   ///< A line sticks: one protection register reads a fixed value.
constexpr uint8_t kUnstick = 0xFB; ///< The line is free again.

/// The registers a stuck line is placed on: the four whose bits switch a protection off.
constexpr std::array<uint8_t, 4> kStuckSet = {0x91, 0x89, 0x8A, 0x92};

/// The port: the chip model of the host tests behind a bus the fuzzer controls.
class FuzzPort : public pf1550::I2cInterface<FuzzPort> {
public:
  explicit FuzzPort(Input& in) noexcept : in_(in) {}

  [[nodiscard]] bool EnsureInitialized() const noexcept {
    return true;
  }

  bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept {
    Check((data != nullptr) && (len >= 2U));
    const uint8_t action = nextAction();
    if (action == kFail) {
      return false;
    }
    if (action == kFlip) {
      return true; // acknowledged, never stored
    }
    setStuck(action);
    return bus_.DoWrite(addr, data, len);
  }

  bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                 std::size_t rx_len) noexcept {
    Check((tx != nullptr) && (rx != nullptr) && (tx_len == 1U) && (rx_len >= 1U));
    const uint8_t action = nextAction();
    setStuck(action);
    if ((action == kFail) || !bus_.DoWriteRead(addr, tx, tx_len, rx, rx_len)) {
      return false;
    }
    for (std::size_t i = 0; i < rx_len; ++i) {
      const auto at = static_cast<uint8_t>(tx[0] + i);
      const uint8_t clean = rx[i];
      if (action == kFlip) {
        rx[i] = static_cast<uint8_t>(rx[i] ^ in_.Byte());
      } else if (action == kReplace) {
        rx[i] = in_.Byte();
      } else {
        // Pass the byte through unchanged.
      }
      noteRead(at, clean, rx[i]);
    }
    return true;
  }

  void DelayUs(uint32_t us) noexcept {
    bus_.chip.Elapse(us);
  }

  bool GpioSet(pf1550::CtrlPin pin, pf1550::GpioSignal signal) noexcept {
    (void)signal;
    return (pin == pf1550::CtrlPin::Standby) && (nextAction() != kFail);
  }

  bool GpioGet(pf1550::CtrlPin pin, bool& asserted) noexcept {
    if ((pin != pf1550::CtrlPin::Intb) || (nextAction() == kFail)) {
      return false;
    }
    asserted = bus_.chip.IntbAsserted();
    return true;
  }

  // The port hook must be a non-const `bool GetLastFailure(I2cFailure&) noexcept`.
  // NOLINTNEXTLINE(readability-make-member-function-const)
  bool GetLastFailure(pf1550::I2cFailure& why) noexcept {
    why = bus_.last_failure;
    return true;
  }

  /// The driver logs plain text: one non-empty line, never a format string.
  void Log(pf1550::LogLevel level, const char* message) noexcept {
    (void)level;
    Check((message != nullptr) && (std::strlen(message) > 0U));
  }

  pf1550_test::Pf1550Model& Chip() noexcept {
    return bus_.chip;
  }
  /// Transfers and pin accesses so far.
  [[nodiscard]] int Transfers() const noexcept {
    return transfers_;
  }
  /// Transfers the fuzzer disturbed so far (failed, flipped, replaced, stuck).
  [[nodiscard]] int Faults() const noexcept {
    return faults_;
  }

private:
  /// Noise is not repeatable: a corrupted byte never equals the corrupted byte of the read before
  /// it from the same register (two equal reads are what the driver trusts). What does repeat on a
  /// real bus is a line stuck at 0 or at 1; the driver rejects 0xFF, and 0x00 clears set-only bits.
  void noteRead(uint8_t at, uint8_t clean, uint8_t& value) noexcept {
    if (static_cast<uint32_t>(at) == bus_.stuck_addr) {
      return; // a stuck line repeats: that is the case the equality of two reads cannot catch
    }
    const bool bad = (value != clean);
    if (bad && last_bad_ && (at == last_at_) && (value == last_value_)) {
      value = static_cast<uint8_t>(value ^ 0x01U);
      if (value == clean) {
        value = static_cast<uint8_t>(value ^ 0x02U);
      }
    }
    last_bad_ = bad;
    last_at_ = at;
    last_value_ = value;
  }

  void setStuck(uint8_t action) noexcept {
    if (action == kStuck) {
      bus_.stuck_addr = kStuckSet[in_.Byte() % kStuckSet.size()];
      bus_.stuck_value = in_.Byte();
    } else if (action == kUnstick) {
      bus_.stuck_addr = 0x100U;
    } else {
      // Any other action leaves the line as it is.
    }
  }

public:
  /// The register a line is stuck on, 0x100 when none.
  [[nodiscard]] uint32_t StuckAt() const noexcept {
    return bus_.stuck_addr;
  }

private:
  uint8_t nextAction() noexcept {
    transfers_++;
    const uint8_t action = in_.Byte();
    if ((action == kFail) || (action == kFlip) || (action == kReplace) || (action == kStuck)) {
      faults_++;
    }
    return action;
  }

  Input& in_;
  pf1550_test::MockBusState bus_;
  int transfers_ = 0;
  int faults_ = 0;
  bool last_bad_ = false;
  uint8_t last_at_ = 0;
  uint8_t last_value_ = 0;
};

using Driver = pf1550::PF1550<FuzzPort>;

bool KnownError(Error e) noexcept {
  return (static_cast<uint8_t>(e) <= static_cast<uint8_t>(Error::Unsupported)) ||
         ((static_cast<uint8_t>(e) >= 16U) && (static_cast<uint8_t>(e) <= 23U));
}

/// A configuration from the fuzzer's bytes; always inside the schema, sometimes unsafe on purpose.
pf1550::Config FuzzConfig(Input& in, uint8_t option) noexcept {
  pf1550::Config c = pf1550_test::BaseConfig();
  const uint8_t flags = in.Byte();
  c.ack.dvs_on_unlisted_part = true;
  if ((flags & 0x40U) != 0U) { // the application accepted every downgrade
    c.ack.allow_untimed_charge = true;
    c.ack.battery_has_own_ntc_protection = true;
    c.ack.allow_batfet_oc_off = true;
    c.ack.allow_batfet_off = true;
    c.ack.hard_reset_paths = true;
    c.ack.undervoltage_threshold = true;
  } else {
    // A second byte accepts single downgrades, so that every acknowledgement is tried alone.
    const uint8_t acks = in.Byte();
    c.ack.allow_untimed_charge = (acks & 0x01U) != 0U;
    c.ack.battery_has_own_ntc_protection = (acks & 0x02U) != 0U;
    c.ack.allow_batfet_oc_off = (acks & 0x04U) != 0U;
    c.ack.allow_batfet_off = (acks & 0x08U) != 0U;
    c.ack.hard_reset_paths = (acks & 0x10U) != 0U;
    c.ack.undervoltage_threshold = (acks & 0x20U) != 0U;
  }
  if ((flags & 0x80U) != 0U) { // the power block, with the guarded fields
    c.power.apply = true;
    c.power.write_reset_paths = true;
    c.power.write_uvdet = true;
    c.power.onkey_reset = true;
  }
  if ((flags & 0x01U) != 0U) {
    c.charger.apply = true;
    c.charger.charge_voltage_mv = static_cast<uint16_t>(3500U + (20U * (in.Byte() % 64U)));
    c.charger.charge_current_ma = static_cast<uint16_t>(100U + (50U * (in.Byte() % 19U)));
    c.charger.input_limit_ma = 1000;
    c.charger.mode =
        ((flags & 0x02U) != 0U) ? pf1550::ChargerMode::Charge : pf1550::ChargerMode::Off;
  }
  c.sw1.apply = (flags & 0x04U) != 0U;
  // Table 85: A1 starts SW1 non-DVS, A2 and A3 in DVS; the others are not listed (acknowledged).
  const bool sw1_dvs = (option != 1U);
  c.sw1.dvs = sw1_dvs ? pf1550::Dvs::Enabled : pf1550::Dvs::Disabled;
  c.sw1.modes.run = true;
  const pf1550::Microvolts sw1_volt = pf1550::FromMillivolts(1000U + (50U * (in.Byte() % 8U)));
  c.sw1.run_uv = sw1_dvs ? sw1_volt : pf1550::Microvolts{};
  c.ldo1.apply = (flags & 0x08U) != 0U;
  c.ldo1.volt = pf1550::FromMillivolts(1700U + (100U * (in.Byte() % 20U)));
  c.ldo1.modes.run = true;
  c.apply_irq_mask = (flags & 0x10U) != 0U;
  if ((flags & 0x20U) != 0U) {
    c.as_found_policy = pf1550::AsFoundPolicy::MakeSafe;
  }
  return c;
}

/// Change the world between two calls.
void FuzzWorld(pf1550_test::Pf1550Model& chip, Input& in, uint8_t option) noexcept {
  switch (in.Byte() % 9U) {
  case 0:
    chip.env.vbus_valid = !chip.env.vbus_valid;
    break;
  case 1:
    chip.env.onkey_pushed = !chip.env.onkey_pushed;
    break;
  case 2:
    chip.env.thermal_shutdown = !chip.env.thermal_shutdown;
    break;
  case 3:
    chip.env.timer_fault = !chip.env.timer_fault;
    break;
  case 4:
    chip.env.battery_mv = 2500U + (50U * in.Byte());
    break;
  case 5: {
    const uint8_t address = in.Byte();
    chip.Poke(address, in.Byte());
    break;
  }
  case 6: {
    const auto reg =
        static_cast<uint8_t>(0x80U + (in.Byte() % 4U)); // one input read per statement (RD-34)
    const auto bit = static_cast<uint8_t>(in.Byte() % 8U);
    chip.Raise(reg, bit);
    break;
  }
  case 7:
    chip.VbusRemoved(); // a cable removal: only VBUS_LIN_ILIM goes back to its OTP value
    break;
  default:
    // A reset restores the part's own start-up option: the OTP is not rewritten. (The byte is still
    // consumed, so stored inputs keep their meaning.)
    static_cast<void>(in.Byte());
    chip.Reset(option);
    break;
  }
}

/// State kept across calls for the whole-run promises.
struct Run {
  bool ship_ok = false;
  bool core_off_ok = false;
  bool load_switch_call =
      false; ///< This call was `EnableLoadSwitch()` (it may fail after the write).
  std::size_t checked_writes = 0;
  uint8_t option = 0;       ///< The start-up option the run began with.
  bool off_request = false; ///< This call was `SetRailModes()` switching a rail off everywhere.
  // A bit the last call set and verified, so the driver holds it as a sentinel: if the chip then
  // reads differently there (a reset, or any other writer), `SelfCheck()` must not say Ok.
  bool armed = false;
  uint8_t armed_addr = 0;
  uint8_t armed_mask = 0;
  uint8_t armed_value = 0;
  pf1550::Rail rail = pf1550::Rail::Sw1; ///< The rail of the last `SetRailModes()` call.
  // A charger status byte the driver read (it shows in `GetLatchedEvents()`: a `ReadRegister()`
  // of CHG_INT, or a `ServiceInterrupts()` that failed after the read) owes the input limit check
  // (header of `ServiceInterrupts()`): the next service that returns Ok has passed it, so after a
  // clean one the limit holds although its own result may show no charger bit (the bits may have
  // been dropped by `ClearLatchedEvents()`). The latch is what the driver saw, so a read the bus
  // flipped owes exactly what the driver noted. No exemption is left for a raw CHG_INT read.
  bool limit_owed = false;
  bool watchdog_clean = false; ///< The last call was a limit-checking service (watchdog, status,
                               ///< charger interrupt) that returned Ok, bus clean.
};

/// The control registers of the rails whose voltage register is one (LDO1 to LDO3 and SW3).
constexpr std::array<pf1550::Rail, 6> kWindowRails = {pf1550::Rail::Sw1,  pf1550::Rail::Sw2,
                                                      pf1550::Rail::Sw3,  pf1550::Rail::Ldo1,
                                                      pf1550::Rail::Ldo2, pf1550::Rail::Ldo3};

[[nodiscard]] bool Newly(uint32_t before, uint32_t after, uint32_t bits) noexcept {
  return ((after & bits) != 0U) && ((before & bits) == 0U);
}

/// The DVS mode the driver works with for SW1 or SW2: Table 85 for options A1 to A3, else the
/// application's declaration.
[[nodiscard]] bool DvsOf(const pf1550_test::Pf1550Model& chip, const pf1550::Config& cfg,
                         uint8_t option, pf1550::Rail rail) noexcept {
  const bool sw1 = (rail == pf1550::Rail::Sw1);
  if (!sw1 && (rail != pf1550::Rail::Sw2)) {
    return false;
  }
  if ((option >= 1U) && (option <= 3U)) {
    return sw1 ? chip.Row().sw1_dvs : chip.Row().sw2_dvs;
  }
  return (sw1 ? cfg.sw1.dvs : cfg.sw2.dvs) == pf1550::Dvs::Enabled;
}

/// The held voltage of a rail the call enabled lies inside its window: the register of the state
/// that was enabled (the run voltage, or a DVS buck's standby or sleep voltage).
void CheckEnabledRail(FuzzPort& port, const pf1550::Config& cfg, pf1550::Rail rail, uint8_t option,
                      uint32_t newly) noexcept {
  auto& chip = port.Chip();
  const pf1550::detail::RailRegs& regs = pf1550::detail::RegsOf(rail);
  const pf1550::RailLimits& lim = pf1550::detail::LimitsOf(cfg.limits, rail);
  const bool dvs = DvsOf(chip, cfg, option, rail);
  uint8_t address = regs.volt;
  if (dvs && (regs.stby_volt != 0U)) {
    if ((newly & pf1550::reg::kCtrlEn) != 0U) {
      address = regs.volt;
    } else if ((newly & pf1550::reg::kCtrlStbyEn) != 0U) {
      address = regs.stby_volt;
    } else {
      address = regs.slp_volt;
    }
  }
  if (port.StuckAt() == address) {
    return; // two equal wrong reads cannot be told from the chip's own value
  }
  const uint32_t mv = pf1550::detail::VoltUv(rail, chip.Peek(address), dvs) / 1000U;
  Check((lim.max_mv != 0U) && (mv >= lim.min_mv) && (mv <= lim.max_mv));
}

/// The protections and guarded fields: what one write may newly change, given the acknowledgements.
void CheckProtection(const pf1550_test::WriteRecord& w, const pf1550::Config& cfg) noexcept {
  namespace reg = pf1550::reg;
  const pf1550::Acknowledgements& ack = cfg.ack;
  switch (w.address) {
  case reg::kBatfetCnfg:
    Check(ack.allow_batfet_oc_off || (reg::BatfetOc::Extract(w.value) != 0U) ||
          (reg::BatfetOc::Extract(w.before) == 0U));
    break;
  case reg::kChgOper:
    Check(ack.allow_batfet_off || !Newly(w.before, w.value, reg::kChgOperDisbatfet));
    break;
  case reg::kChgTmr:
    Check(ack.allow_untimed_charge || (reg::Fchgtime::Extract(w.value) != 0U) ||
          (reg::Fchgtime::Extract(w.before) == 0U));
    break;
  case reg::kThmRegCnfg:
    Check(ack.battery_has_own_ntc_protection || (reg::ThmCnfg::Extract(w.value) != 0U) ||
          (reg::ThmCnfg::Extract(w.before) == 0U));
    break;
  case reg::kPwrCtrl0:
    Check(ack.hard_reset_paths ||
          (reg::TgReset::Extract(w.value) == reg::TgReset::Extract(w.before)));
    break;
  case reg::kPwrCtrl1: {
    constexpr uint32_t kPaths = static_cast<uint32_t>(reg::kPwronRstEn) |
                                static_cast<uint32_t>(reg::kRestartEn) |
                                static_cast<uint32_t>(reg::kOnkeyRstEn);
    Check(ack.hard_reset_paths || ((static_cast<uint32_t>(w.value ^ w.before) & kPaths) == 0U));
    break;
  }
  case reg::kPwrCtrl2:
    Check(ack.undervoltage_threshold ||
          (reg::Uvdet::Extract(w.value) == reg::Uvdet::Extract(w.before)));
    break;
  case reg::kPwrCtrl3: {
    constexpr uint32_t kBoth =
        static_cast<uint32_t>(reg::kGotoShip) | static_cast<uint32_t>(reg::kGotoCoreOff);
    Check((static_cast<uint32_t>(w.value) & kBoth) != kBoth);
    break;
  }
  default:
    break;
  }
}

/// Whether a write to VBUS_INLIM_CNFG was made since write number `from`.
bool WroteLimitSince(pf1550_test::Pf1550Model& chip, std::size_t from) noexcept {
  for (std::size_t i = from; i < chip.writes.size(); ++i) {
    if (chip.writes[i].address == 0x94U) {
      return true;
    }
  }
  return false;
}

/// The promises that hold after every call, whatever it was.
void CheckAfter(FuzzPort& port, Run& run, const pf1550::Config& cfg) noexcept {
  auto& chip = port.Chip();
  if (run.watchdog_clean) {
    // A watchdog service that said Ok with the charger in Charge: the input limit the chip holds is
    // the configured one when the charger block is applied (a cable removal puts the OTP value
    // back; while VBUS_SNS shows VBUSIN invalid the restore waits), else a valid one inside Limits.
    run.watchdog_clean = false;
    if (pf1550::detail::ModeOf(chip.Peek(pf1550::reg::kChgOper)) == pf1550::ChargerMode::Charge) {
      const uint32_t code = pf1550::reg::VbusLinIlim::Extract(chip.Peek(0x94));
      const pf1550::Result<uint32_t> ma = pf1550::InputLimitMa(code);
      if (cfg.charger.apply) {
        // The driver can only go by the VBUS_SNS register (a poke can make it differ from the
        // world); decoded here on its own: bit 5 is VBUS_VALID (Table 153).
        const bool vbus_valid = (static_cast<uint32_t>(chip.Peek(0x86)) & 0x20U) != 0U;
        Check(!vbus_valid || (ma.ok() && (ma.value == cfg.charger.input_limit_ma)));
      } else {
        Check(ma.ok() && (ma.value <= cfg.limits.input_max_ma));
      }
    }
  }
  Check(chip.protected_writes == 0U);
  Check(run.ship_ok || !chip.ship_entered);
  Check(run.core_off_ok || !chip.core_off_entered);
  for (const pf1550::Rail rail : kWindowRails) {
    // A rail enabled by a write of this call: its held voltage was in the window.
    const uint8_t ctrl = pf1550::detail::RegsOf(rail).ctrl;
    for (std::size_t i = run.checked_writes; i < chip.writes.size(); ++i) {
      const auto& w = chip.writes[i];
      constexpr uint32_t kEnables = static_cast<uint32_t>(pf1550::reg::kCtrlEn) |
                                    static_cast<uint32_t>(pf1550::reg::kCtrlStbyEn) |
                                    static_cast<uint32_t>(pf1550::reg::kCtrlOmode);
      if ((w.address == ctrl) && Newly(w.before, w.value, kEnables)) {
        const uint32_t newly =
            static_cast<uint32_t>(w.value) & ~static_cast<uint32_t>(w.before) & kEnables;
        CheckEnabledRail(port, cfg, rail, run.option, newly);
      }
    }
  }
  for (; run.checked_writes < chip.writes.size(); ++run.checked_writes) {
    const auto& w = chip.writes[run.checked_writes];
    CheckProtection(w, cfg);
    if (((w.address == pf1550::reg::kLdo1Ctrl) || (w.address == pf1550::reg::kLdo3Ctrl)) &&
        Newly(w.before, w.value, pf1550::reg::kLdoLsEn)) {
      Check(run.load_switch_call);
    }
    if (w.address == pf1550::reg::kBattReg) {
      Check(pf1550::ChargeVoltageMv(w.value & 0x3FU) <= cfg.limits.cell_max_mv);
    } else if (w.address == pf1550::reg::kChgCurrCnfg) {
      const auto ma = pf1550::ChargeCurrentMa(w.value & 0x1FU);
      Check(!ma.ok() || (ma.value <= cfg.limits.cell_max_charge_ma));
    } else {
      // Other registers carry no limit of their own.
    }
  }
}

/// After a `SetRailModes()` that returned Ok and set mode bits of a rail control register (or
/// switched it off), the driver holds them as a sentinel: note what the chip must still read.
void Arm(FuzzPort& port, Run& run, Error error, std::size_t first_write) noexcept {
  if (error != Error::Ok) {
    return;
  }
  const uint8_t ctrl = pf1550::detail::RegsOf(run.rail).ctrl;
  constexpr auto kMask = static_cast<uint8_t>(static_cast<uint32_t>(pf1550::reg::kCtrlStbyEn) |
                                              static_cast<uint32_t>(pf1550::reg::kCtrlOmode));
  uint32_t changed = 0U;
  uint8_t last = 0U;
  bool wrote = false;
  const auto& writes = port.Chip().writes;
  for (std::size_t i = first_write; i < writes.size(); ++i) {
    if (writes[i].address == ctrl) {
      changed |= (static_cast<uint32_t>(writes[i].before) ^ static_cast<uint32_t>(writes[i].value));
      last = writes[i].value;
      wrote = true;
    }
  }
  changed &= static_cast<uint32_t>(kMask);
  if (wrote && ((changed != 0U) || run.off_request)) {
    run.armed = true;
    run.armed_addr = ctrl;
    // Only bits the driver changed are evidence: a bit it left alone is not a sentinel (a rail
    // switched off everywhere is the exception: every mode bit is written and verified).
    run.armed_mask = run.off_request ? kMask : static_cast<uint8_t>(changed);
    run.armed_value = static_cast<uint8_t>(static_cast<uint32_t>(last) & run.armed_mask);
  }
}

/// A world change that leaves the armed bits different (a chip reset puts them back to the OTP
/// image) must be seen by the driver's reset check, unless the bus itself misbehaved meanwhile.
void CheckResetSeen(Driver& d, FuzzPort& port, Run& run) noexcept {
  if (!run.armed || !d.IsInitialized()) {
    return;
  }
  const uint32_t now = port.Chip().Peek(run.armed_addr);
  if (((now ^ static_cast<uint32_t>(run.armed_value)) & static_cast<uint32_t>(run.armed_mask)) ==
      0U) {
    return;
  }
  // A line stuck on a register repeats a value for every read, the right one included.
  const bool stuck = port.StuckAt() != 0x100U;
  const int faults = port.Faults();
  const auto report = d.SelfCheck();
  if ((port.Faults() == faults) && !stuck && (port.StuckAt() == 0x100U)) {
    Check(!report.ok());
  }
  run.armed = false;
}

void RunOne(Driver& d, FuzzPort& port, Run& run, Input& in) noexcept {
  const uint8_t op = in.Byte() % 24U; // 22: a change of the world, 23: the latch is cleared
  run.load_switch_call = false;
  run.off_request = false;
  if (op != 22U) {
    run.armed = false; // a call that is not a world change moves the sentinels
  }
  const bool was_up = d.IsInitialized();
  const int before = port.Transfers();
  const std::size_t writes_before = port.Chip().writes.size();
  Error error = Error::Ok;

  switch (op) {
  case 0:
    error = d.EnsureInitialized().error;
    break;
  case 1:
    error = d.Configure().error;
    break;
  case 2:
    error = d.ReadVariant().error;
    break;
  case 3:
    error = d.ReadSnapshot().error;
    break;
  case 4:
    error = d.SelfCheck().error;
    break;
  case 5: {
    const int faults = port.Faults();
    const uint32_t stuck0 =
        port.StuckAt(); // a line unstuck during the call was stuck for its reads
    const std::size_t from = port.Chip().writes.size();
    error = d.ReadChargerStatus().error;
    // A status read checks the input limit every time; after a reset it writes none.
    run.watchdog_clean = (error == Error::Ok) && (port.Faults() == faults) &&
                         (port.StuckAt() == 0x100U) && (stuck0 == 0x100U);
    Check((error != Error::DeviceReset) || !WroteLimitSince(port.Chip(), from));
    break;
  }
  case 6:
    error = d.SetChargerMode(static_cast<pf1550::ChargerMode>(in.Byte() % 5U)).error;
    break;
  case 7:
    error = d.RestartCharger().error;
    break;
  case 8: {
    const int faults = port.Faults();
    const uint32_t stuck0 =
        port.StuckAt(); // a line unstuck during the call was stuck for its reads
    error = d.ServiceChargerWatchdog().error;
    // A line stuck on a protection register is a fault that lasts: the reads cannot be trusted.
    run.watchdog_clean = (error == Error::Ok) && (port.Faults() == faults) &&
                         (port.StuckAt() == 0x100U) && (stuck0 == 0x100U);
    break;
  }
  case 9:
    error = d.ReadRail(static_cast<pf1550::Rail>(in.Byte() % 8U)).error;
    break;
  case 10: {
    pf1550::RailModes m{};
    const uint8_t b = in.Byte();
    m.run = (b & 1U) != 0U;
    m.standby = (b & 2U) != 0U;
    m.sleep = (b & 4U) != 0U;
    m.low_power = (b & 8U) != 0U;
    run.off_request = !m.run && !m.standby && !m.sleep;
    run.rail = static_cast<pf1550::Rail>(in.Byte() % 8U);
    error = d.SetRailModes(run.rail, m).error;
    Arm(port, run, error, writes_before);
    break;
  }
  case 11: {
    const auto rail = static_cast<pf1550::Rail>(in.Byte() % 8U);
    error = d.SetRailVoltage(rail, pf1550::FromMillivolts(500U + (25U * in.Byte()))).error;
    break;
  }
  case 12: {
    const auto rail = static_cast<pf1550::Rail>(in.Byte() % 8U);
    const pf1550::LoadSwitchConfirm confirm{.input_mv = 1700U + (50U * (in.Byte() % 40U))};
    run.load_switch_call = true;
    error = d.EnableLoadSwitch(rail, confirm).error;
    break;
  }
  case 13:
    error = d.RequestStandby((in.Byte() & 1U) != 0U).error;
    break;
  case 14: {
    const auto r = d.EnterShip(pf1550::ShipConfirm{});
    run.ship_ok = run.ship_ok || r.ok();
    error = r.error;
    break;
  }
  case 15: {
    const auto r = d.EnterCoreOff(pf1550::CoreOffConfirm{});
    run.core_off_ok = run.core_off_ok || r.ok();
    error = r.error;
    break;
  }
  case 16: {
    const int faults = port.Faults();
    const uint32_t stuck0 =
        port.StuckAt(); // a line unstuck during the call was stuck for its reads
    const std::size_t from = port.Chip().writes.size();
    const auto events = d.ServiceInterrupts();
    const bool owed = run.limit_owed;
    run.limit_owed = !events.ok() && (owed || (d.GetLatchedEvents().status[0] != 0U));
    error = events.error;
    // A charger interrupt starts the input limit check: when it was served cleanly the limit holds.
    run.watchdog_clean =
        events.ok() && (port.Faults() == faults) && (port.StuckAt() == 0x100U) &&
        (stuck0 == 0x100U) &&
        (owed || (((events.value.categories & 1U) != 0U) && (events.value.status[0] != 0U)));
    Check((error != Error::DeviceReset) || !WroteLimitSince(port.Chip(), from));
    break;
  }
  case 17: {
    pf1550::IrqMask mask{};
    for (auto& b : mask.masked) {
      b = in.Byte();
    }
    error = d.SetIrqMask(mask).error;
    break;
  }
  case 18:
    error = d.ReadPowerDownSequence().error;
    break;
  case 19:
    error = d.CancelShutdownRequest().error;
    break;
  case 20:
    error = d.CheckChargerAgainstLimits().error;
    break;
  case 21: {
    const uint8_t address = in.Byte();
    error = d.ReadRegister(address).error;
    run.limit_owed = run.limit_owed || (d.GetLatchedEvents().status[0] != 0U);
    break;
  }
  case 23:
    d.ClearLatchedEvents(); // no I/O: it answers nothing, and the latch is empty afterwards
    Check(port.Transfers() == before);
    Check(!d.GetLatchedEvents().fault_class);
    for (const uint8_t status : d.GetLatchedEvents().status) {
      Check(status == 0U);
    }
    return; // an owed check stays owed: the latch is gone, the pending flag is not

  default:
    FuzzWorld(port.Chip(), in, run.option);
    CheckResetSeen(d, port, run);
    return;
  }

  Check(KnownError(error));
  // Switching the charger off, cancelling a shutdown request and the as-found check work once the
  // chip has been identified, even when start-up then failed; every other call needs a started
  // driver and sends nothing before that.
  const bool identified_only =
      (op == 6U) || (op == 7U) || (op == 19U) || (op == 20U) || ((op == 10U) && run.off_request);
  if (!was_up && (op != 0U) && !identified_only) {
    Check((error == Error::NotInitialized) && (port.Transfers() == before));
  }
  if (error == Error::NotInitialized) {
    Check(!d.IsInitialized());
    if (!was_up && !identified_only) {
      Check(port.Transfers() == before);
    }
  }
  if (error == Error::DeviceReset) {
    Check(!d.IsInitialized());
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  FuzzPort port(in);
  const auto option = static_cast<uint8_t>(in.Byte() % 10U);
  port.Chip().Reset(option);
  const pf1550::Config cfg = FuzzConfig(in, option);
  Driver driver(port, cfg);
  Run run{};
  run.option = option;
  for (int calls = 0; (calls < 64) && !in.Empty(); ++calls) {
    RunOne(driver, port, run, in);
    CheckAfter(port, run, cfg);
  }
  return 0;
}
