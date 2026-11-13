/**
 * @file mock_pf1550.hpp
 * @brief An independent register model of the PF1550 for the host tests.
 *
 * The model is written from the data sheet, not from the driver: it has its own copy of the
 * start-up options (Table 85), its own access classes, charger state machine, DVS timing, interrupt
 * logic, power-up and power-down sequencer and coin cell charger. It shares nothing with `inc/`
 * except the public enumerations the tests compare against. Where the data sheet is silent or
 * inconsistent the behaviour is a field of `Readings` (ambiguities C1 to C58, docs/ambiguities.md);
 * the tests run the driver under each reading.
 */
#ifndef PF1550_MOCK_PF1550_HPP
#define PF1550_MOCK_PF1550_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pf1550_test {

/// How a status register clears (ambiguity C3).
enum class ClearMode : uint8_t { WriteOne, OnRead, Both };

/// What GOTO_SHIP does with VBUS valid (C6): the part goes to REGS_DISABLE only, ignores the bit,
/// or enters SHIP anyway.
enum class ShipWithVbus : uint8_t { RegsDisableOnly, BitIgnored, ShipsAnyway };

/// The readings of every ambiguity the data sheet leaves open; each is a knob of the model.
struct Readings {
  ClearMode int_clear = ClearMode::WriteOne; ///< C3.
  bool sw3_write_ignored = true;     ///< C2: false stores a write to the SW3 voltage registers.
  bool rw1s_set_only = false;        ///< C4: a written 0 does not clear an OTP-loaded bit.
  bool wd_time_one_is_32s = true;    ///< C8.
  bool wdt_reports_0x0b = false;     ///< C9: CHG_SNS 0x0B on a watchdog expiry.
  uint8_t silicon_rev_reset = 0x11;  ///< C22: 0x11 or 0x08.
  bool nondvs_write_accepted = true; ///< C35: DVS registers of a non-DVS buck take a write.
  bool core_off_needs_run = true;    ///< C7: GOTO_CORE_OFF acts only in a system-on state.
  uint32_t dvs_done_delay_us = 100;  ///< C34: when the DVS-done flag rises after a step.
  bool dvs_never_done = false;       ///< C34: the DVS-done flag never rises.
  uint32_t apply_delay_us = 0;     ///< C49: charger parameters take effect this long after a write.
  bool seq0_starts_first = true;   ///< C54: PWRUP_SEQ 0 starts in the first slot.
  bool seq_order_zero_last = true; ///< C56: with other orders set, order 0 turns off last.
  bool ls_fault_interrupt = false; ///< C31.
  ShipWithVbus ship_with_vbus = ShipWithVbus::RegsDisableOnly; ///< C6.
  bool pwrup_flag_at_reset = false; ///< C59: a reset or power-up sets PWRUP_I too.
  bool wdtclr_holds = false; ///< C58: WDTCLR stores what is written (plain RW, Table 161) and
                             ///< only a 0 to 01 edge clears the watchdog.
};

/// What the world around the chip does; the tests set it.
struct Environment {
  bool vbus_valid = false;         ///< A valid supply on VBUSIN.
  bool vbus_limit_held = false;    ///< VBUSIN stays invalid: VBUS_LIN_ILIM is held in reset (Table
                                   ///< 163) and a write to it does not hold.
  bool vbus_ovlo = false;          ///< Above OVLO.
  bool vbus_dpm = false;           ///< Input voltage regulation active.
  uint32_t battery_mv = 3800;      ///< Cell voltage.
  bool battery_present = true;     ///< A cell is connected.
  bool onkey_pushed = false;       ///< ONKEY pulled low.
  bool pwron_high = true;          ///< PWRON level.
  bool timer_fault = false;        ///< A charge timer expired.
  bool thermal_shutdown = false;   ///< Junction above TSHDN.
  bool thermistor_outside = false; ///< Battery temperature outside the window.
  bool battery_oc = false;         ///< Battery discharge overcurrent.
  bool battery_ov = false;         ///< Battery overvoltage.
  bool watchdog_expired = false;   ///< Charger watchdog expired.
};

/// Table 85 for one option: the values the registers hold after start-up.
struct OptionRow {
  uint8_t sw1_volt, sw2_volt, sw3_volt, ldo1_volt, ldo2_volt, ldo3_volt;
  std::array<uint8_t, 7> seq; ///< PWRUP_SEQ for SW1, SW2, SW3, LDO1, LDO2, LDO3, VREFDDR.
  bool sw1_dvs, sw2_dvs;      ///< The model's reading of OTP_SWx_DVS_SEL (ambiguity C1).
  bool ldo3_load_switch;
  bool sw2_speed_2us;
  uint8_t chg_oper, chg_cc, vsysmin, ilim, dpm;
  bool usbphyldo, pwron_edge, slot_fast;
};

/// The ten options A0 to A9 (A0 is not programmed: all zero).
inline constexpr std::array<OptionRow, 10> kOptions = {{
    {0,
     0,
     0,
     0,
     0,
     0,
     {0, 0, 0, 0, 0, 0, 0},
     false,
     false,
     false,
     false,
     0,
     0,
     0,
     0,
     0,
     false,
     false,
     false},
    {0,
     40,
     0,
     5,
     7,
     5,
     {1, 2, 3, 4, 4, 5, 5},
     false,
     true,
     true,
     true,
     2,
     0,
     2,
     14,
     0,
     false,
     false,
     true},
    {32,
     1,
     0,
     16,
     15,
     16,
     {5, 5, 1, 1, 1, 1, 5},
     true,
     false,
     false,
     false,
     1,
     0,
     2,
     20,
     0,
     true,
     false,
     false},
    {63,
     60,
     15,
     16,
     15,
     31,
     {3, 3, 3, 3, 2, 3, 3},
     true,
     true,
     false,
     false,
     2,
     8,
     2,
     20,
     0,
     true,
     false,
     false},
    {40,
     48,
     0,
     31,
     15,
     16,
     {4, 3, 2, 1, 2, 1, 3},
     true,
     true,
     false,
     false,
     2,
     8,
     2,
     20,
     6,
     true,
     false,
     false},
    {63,
     3,
     15,
     16,
     15,
     31,
     {3, 3, 3, 3, 2, 3, 3},
     true,
     false,
     false,
     false,
     2,
     8,
     2,
     20,
     0,
     true,
     false,
     false},
    {54,
     2,
     15,
     16,
     15,
     31,
     {3, 3, 3, 3, 2, 3, 3},
     true,
     false,
     false,
     false,
     1,
     8,
     1,
     20,
     0,
     true,
     false,
     false},
    {63,
     1,
     0,
     31,
     15,
     31,
     {3, 3, 3, 3, 2, 3, 3},
     true,
     false,
     false,
     false,
     2,
     8,
     2,
     20,
     0,
     true,
     false,
     false},
    {63,
     2,
     15,
     16,
     15,
     31,
     {3, 3, 3, 3, 2, 3, 3},
     true,
     false,
     false,
     false,
     2,
     8,
     2,
     20,
     0,
     true,
     true,
     false},
    {7,
     7,
     12,
     26,
     15,
     16,
     {3, 3, 3, 3, 3, 3, 3},
     false,
     false,
     false,
     false,
     1,
     0,
     1,
     20,
     0,
     true,
     false,
     false},
}};

/// One recorded register write.
struct WriteRecord {
  uint8_t address;
  uint8_t value;
  uint8_t before; ///< What the register held before the write.
};

/// The writes a chip received, in order: a fixed-size log (the first 16384 are kept), so the tests
/// do not depend on a growing container (GCC's -O2 flow analysis reports false overflows on
/// `push_back` here).
class WriteLog {
public:
  static constexpr std::size_t kCapacity = 16384;
  void PushBack(const WriteRecord& w) noexcept {
    if (size_ < kCapacity) {
      items_[size_] = w;
      ++size_;
    }
  }
  void Clear() noexcept {
    size_ = 0U;
  }
  [[nodiscard]] std::size_t size() const noexcept {
    return size_;
  }
  [[nodiscard]] const WriteRecord& operator[](std::size_t i) const noexcept {
    return items_[i % kCapacity];
  }
  [[nodiscard]] const WriteRecord* begin() const noexcept {
    return items_.data();
  }
  [[nodiscard]] const WriteRecord* end() const noexcept {
    return items_.data() + size_;
  }

private:
  std::array<WriteRecord, kCapacity> items_{};
  std::size_t size_{0U};
};

/// A rail event of the sequencer model.
struct SequenceEvent {
  uint8_t rail; ///< 0 SW1, 1 SW2, 2 SW3, 3 LDO1, 4 LDO2, 5 LDO3, 6 VREFDDR.
  uint32_t
      time_us; ///< From the start of the first slot (power-up) or of the sequence (power-down).
};

/// The chip.
class Pf1550Model {
public:
  Readings readings;             ///< Ambiguity knobs.
  Environment env;               ///< The world.
  uint32_t protected_writes = 0; ///< Writes that hit a read-only, key or undocumented register.
  uint32_t wdt_clears = 0;       ///< Watchdog clears seen.
  bool ship_entered = false;     ///< The chip went to SHIP.
  bool core_off_entered = false; ///< The chip went to CORE_OFF.
  WriteLog writes;               ///< Every write in order.

  explicit Pf1550Model(uint8_t option = 2) noexcept {
    Reset(option);
  }

  /// Power-on reset into `option` (0 to 9).
  void Reset(uint8_t option) noexcept {
    option_ = option;
    Reset();
  }

  /// The charge voltage the registers hold, mV (BATT_REG.CHGCV, code 8 is 3.5 V, 20 mV a step).
  [[nodiscard]] uint32_t AppliedCvMv() const noexcept {
    const uint32_t code = regs_[0x8F] & 0x3FU;
    const uint32_t clamped = (code > 55U) ? 55U : code;
    return 3500U + (20U * ((clamped < 8U) ? 0U : (clamped - 8U)));
  }
  /// The fast charge current the registers hold, mA (CHG_CURR_CNFG.CHG_CC, 0 for a reserved code).
  [[nodiscard]] uint32_t AppliedCcMa() const noexcept {
    const uint32_t code = regs_[0x8E] & 0x1FU;
    return (code > 18U) ? 0U : (100U + (50U * code));
  }
  /// USB_PHY_LDO_CNFG.USBPHYLDO: the USB PHY supply is on (state only, the analog is not modelled).
  [[nodiscard]] bool UsbphyEnabled() const noexcept {
    return (regs_[0x96] & 0x04U) != 0U;
  }
  /// USB_PHY_LDO_CNFG.USBPHY: the PHY supply voltage, mV (0 for 3.3 V, 1 for 4.9 V).
  [[nodiscard]] uint32_t UsbphyMv() const noexcept {
    return ((regs_[0x96] & 0x02U) != 0U) ? 4900U : 3300U;
  }
  /// The LED driver as the registers set it (Tables 170 to 172): state only, the output is not
  /// simulated.
  struct LedState {
    bool enabled;
    bool software_override;
    bool flash_while_charging;
    uint32_t duty_32nds;
  };
  [[nodiscard]] LedState Led() const noexcept {
    return LedState{(regs_[0x9C] & 0x80U) != 0U, (regs_[0x9E] & 0x20U) != 0U,
                    (regs_[0x9E] & 0x10U) != 0U, static_cast<uint32_t>(regs_[0x9C] & 0x3FU)};
  }

  /// The start-up row (Table 85) of the option the model holds.
  [[nodiscard]] const OptionRow& Row() const noexcept {
    return kOptions[option_ % kOptions.size()];
  }

  /// An invalid VBUSIN: only the fields whose reset condition is CHGPOK_RSTB go back to their OTP
  /// value (Table 163: VBUS_LIN_ILIM). Everything else keeps its value (ambiguity C14).
  void VbusRemoved() noexcept {
    const OptionRow& o = kOptions[option_ % kOptions.size()];
    regs_[0x94] = static_cast<uint8_t>((static_cast<uint32_t>(regs_[0x94]) & 0x07U) |
                                       (static_cast<uint32_t>(o.ilim) << 3));
    refresh();
  }

  /// Reset to the option the model holds (a VCOREDIG or POR reset: every register).
  void Reset() noexcept {
    regs_.fill(0);
    pinned_.fill(false);
    const OptionRow& o = kOptions[option_ % kOptions.size()];
    regs_[0x00] = 0x7C;
    regs_[0x01] = option_;
    regs_[0x02] = readings.silicon_rev_reset;
    regs_[0x32] = o.sw1_volt;
    regs_[0x38] = o.sw2_volt;
    regs_[0x33] = regs_[0x34] = o.sw1_volt; // Tables 113, 114: the default equals SW1_VOLT
    regs_[0x39] = regs_[0x3A] = o.sw2_volt; // and likewise for SW2
    regs_[0x3E] = o.sw3_volt;
    regs_[0x4C] = o.ldo1_volt;
    regs_[0x4F] = o.ldo2_volt;
    regs_[0x52] = o.ldo3_volt;
    for (const uint8_t a : std::array<uint8_t, 7>{0x35, 0x3B, 0x41, 0x4A, 0x4D, 0x50, 0x53}) {
      regs_[a] = 0x03; // EN and STBY_EN: OTP_xxx_EN_AND_STBY_EN
    }
    regs_[0x35] = static_cast<uint8_t>(regs_[0x35] | 0x10U); // SW1 4 us
    regs_[0x3B] = static_cast<uint8_t>(regs_[0x3B] | (o.sw2_speed_2us ? 0x00U : 0x10U));
    if (o.ldo3_load_switch) {
      regs_[0x53] = static_cast<uint8_t>(regs_[0x53] | 0x10U);
    }
    regs_[0x36] = regs_[0x3C] = regs_[0x42] = 0x00; // ILIM 1.0 A
    const uint8_t pwrdn[7] = {0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65};
    for (std::size_t i = 0; i < 7; ++i) {
      regs_[pwrdn[i]] = o.seq[i]; // the power-down order mirrors the power-up order
    }
    regs_[0x58] = 0x01; // STANDBYDLY 1, POR_DLY 0, TGRESET 0
    regs_[0x59] = 0x80; // ONKEY_RST_EN
    regs_[0x5A] = 0x02; // UVDET 3.0 V rising
    regs_[0x89] = o.chg_oper;
    regs_[0x8A] = 0x00; // FCHGTIME disabled, EOCTIME 16 s, TPRECHG 30 min
    regs_[0x8D] = 0x40; // CHG_RESTART 100 mV, IEOC 50 mA (code 4)
    regs_[0x8E] = o.chg_cc;
    regs_[0x8F] = static_cast<uint8_t>(43U | (static_cast<uint32_t>(o.vsysmin) << 6)); // 4.20 V
    regs_[0x91] = 0x30; // BATFET_OC 3.2 A
    regs_[0x92] = 0x05; // THM_CNFG suspend, REGTEMP 95 C
    regs_[0x94] = static_cast<uint8_t>(o.ilim << 3);
    regs_[0x95] = o.dpm;
    regs_[0x96] = static_cast<uint8_t>((o.usbphyldo ? 0x04U : 0x00U) | 0x01U);
    // PWRUP_I is the STANDBY or SLEEP to RUN completion flag (Table 108), not a power-on flag; a
    // reading in which a power-up sets it too is a knob (C59).
    regs_[0x28] = readings.pwrup_flag_at_reset ? 0x01 : 0x00;
    regs_[0x82] = 0xFF;
    for (const uint8_t a : std::array<uint8_t, 7>{0x09, 0x0C, 0x0F, 0x19, 0x21, 0x25, 0x29}) {
      regs_[a] = 0xFF; // all sources masked at power-up
    }
    state_ = 0x0C;
    ship_entered = core_off_entered = false;
    pending_dvs_ = 0;
    dvs_done_at_ = 0;
    now_us_ = 0;
    refresh();
  }

  /// A register write over I2C.
  void Write(uint8_t address, uint8_t value) noexcept {
    writes.PushBack(
        WriteRecord{address, value, (address < regs_.size()) ? regs_[address] : uint8_t{0}});
    if (address >= regs_.size()) {
      ++protected_writes;
      return;
    }
    if (isReadOnly(address)) {
      ++protected_writes;
      return;
    }
    if (isStatus(address)) {
      regs_[address] = static_cast<uint8_t>(static_cast<uint32_t>(regs_[address]) &
                                            ~static_cast<uint32_t>(value));
    } else if (address == 0x91) {
      writeBatfet(value);
    } else if ((address == 0x94) && env.vbus_limit_held) {
      // held in reset: the limit field keeps its OTP value, the other bits are written
      regs_[address] = static_cast<uint8_t>((static_cast<uint32_t>(value) & 0x07U) |
                                            (static_cast<uint32_t>(regs_[address]) & 0xF8U));
    } else if (address == 0x5B) {
      writeGoto(value);
    } else if (isSw3Volt(address) && readings.sw3_write_ignored) {
      // ignored (C2)
    } else {
      writeRegister(address, value);
    }
    refresh();
  }

  /// A register read over I2C.
  uint8_t Read(uint8_t address) noexcept {
    if (address >= regs_.size()) {
      return 0;
    }
    refresh();
    const uint8_t value = regs_[address];
    if (isStatus(address) && (readings.int_clear != ClearMode::WriteOne)) {
      regs_[address] = 0;
      refresh();
    }
    return value;
  }

  /// Time passes: DVS steps finish.
  void Elapse(uint32_t us) noexcept {
    now_us_ += us;
    if ((pending_dvs_ != 0U) && (now_us_ >= dvs_done_at_) && !readings.dvs_never_done) {
      regs_[0x0E] = static_cast<uint8_t>(regs_[0x0E] | pending_dvs_);
      pending_dvs_ = 0;
      refresh();
    }
  }

  /// Set a register to any value, bypassing the access rules (a test fixture). A register the chip
  /// derives (status, sense, state, category) keeps the poked value until `Unpin()`.
  void Poke(uint8_t address, uint8_t value) noexcept {
    const std::size_t a = address % regs_.size();
    regs_[a] = value;
    pinned_[a] = isDerived(address);
    refresh();
  }

  /// Let the derived registers follow the environment again.
  void Unpin() noexcept {
    pinned_.fill(false);
    refresh();
  }

  /// A register's stored value without side effects.
  [[nodiscard]] uint8_t Peek(uint8_t address) const noexcept {
    return regs_[address % regs_.size()];
  }

  /// Set a status bit as the chip would (the tests drive events through the environment).
  void Raise(uint8_t status_reg, uint8_t bit) noexcept {
    regs_[status_reg] = static_cast<uint8_t>(regs_[status_reg] | (1U << bit));
    refresh();
  }

  /// Set the STATE_INFO code.
  void SetState(uint8_t code) noexcept {
    state_ = code;
    refresh();
  }

  /// The INTB pin: asserted (low) when any unmasked status bit is set.
  [[nodiscard]] bool IntbAsserted() const noexcept {
    static constexpr std::array<uint8_t, 8> kStat = {0x80, 0x08, 0x0B, 0x0E,
                                                     0x18, 0x20, 0x24, 0x28};
    bool pending = false;
    for (const uint8_t s : kStat) {
      const uint32_t mask = regs_[s == 0x80 ? 0x82 : s + 1U];
      pending = pending || ((static_cast<uint32_t>(regs_[s]) & ~mask) != 0U);
    }
    return pending;
  }

  /// The coin cell charge voltage in millivolts, 0 when the charger is off (C40 reading: 100
  /// mV/LSB).
  [[nodiscard]] uint32_t CoinChargeMv() const noexcept {
    return ((regs_[0x30] & 0x10U) != 0U) ? (1800U + (100U * (regs_[0x30] & 0x0FU))) : 0U;
  }

  /// The power-up sequence of this option from a cold start: when each rail turns on, microseconds
  /// from the start of the first slot (Table 84, Table 85).
  [[nodiscard]] std::array<SequenceEvent, 7> PowerUp() const noexcept {
    const OptionRow& o = kOptions[option_ % kOptions.size()];
    const uint32_t slot = o.slot_fast ? 500U : 2000U;
    std::array<SequenceEvent, 7> events{};
    for (uint8_t i = 0; i < 7; ++i) {
      uint32_t n = o.seq[i];
      if (n == 0U) {
        n = readings.seq0_starts_first ? 1U : 0U;
      }
      events[i] = {i, (n > 1U) ? ((n - 1U) * slot) : 0U};
    }
    return events;
  }

  /// RESETBMCU release, microseconds after the last regulator, from POR_DLY (Table 135).
  [[nodiscard]] uint32_t ResetReleaseUs() const noexcept {
    static constexpr std::array<uint32_t, 8> kMs = {2, 2, 4, 8, 16, 128, 256, 1024};
    return kMs[(static_cast<uint32_t>(regs_[0x58]) >> 3U) & 7U] * 1000U;
  }

  /// The power-down sequence as the PWRDN_SEQ registers hold it (section 10.7).
  [[nodiscard]] std::array<SequenceEvent, 7> PowerDown() const noexcept {
    static constexpr std::array<uint8_t, 7> kRegs = {0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65};
    uint32_t top = 0;
    for (const uint8_t a : kRegs) {
      top = (regs_[a] & 7U) > top ? (regs_[a] & 7U) : top;
    }
    std::array<SequenceEvent, 7> events{};
    for (uint8_t i = 0; i < 7; ++i) {
      const uint32_t own = regs_[kRegs[i]] & 7U;
      events[i] = {i, (top - own) * 2000U};
    }
    return events;
  }

  [[nodiscard]] uint8_t Option() const noexcept {
    return option_;
  }

private:
  static bool isDerived(uint8_t a) noexcept {
    return (a == 0x06) || (a == 0x26) || (a == 0x2A) || (a == 0x67) || (a == 0x84) || (a == 0x86) ||
           (a == 0x87) || (a == 0x88);
  }
  static bool isStatus(uint8_t a) noexcept {
    return (a == 0x08) || (a == 0x0B) || (a == 0x0E) || (a == 0x18) || (a == 0x20) || (a == 0x24) ||
           (a == 0x28) || (a == 0x80);
  }
  static bool isSw3Volt(uint8_t a) noexcept {
    return (a == 0x3E) || (a == 0x3F) || (a == 0x40);
  }
  static bool isReadOnly(uint8_t a) noexcept {
    switch (a) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x06:
    case 0x0A:
    case 0x0D:
    case 0x10:
    case 0x1A:
    case 0x22:
    case 0x26:
    case 0x2A:
    case 0x67:
    case 0x68:
    case 0x84:
    case 0x86:
    case 0x87:
    case 0x88:
    case 0x6F:
    case 0x9F:
    case 0x6B:
    case 0x69:
    case 0x6A:
      return true;
    default:
      return !documented(a);
    }
  }
  static bool documented(uint8_t a) noexcept {
    return isStatus(a) || (a == 0x30) || ((a >= 0x32) && (a <= 0x36)) ||
           ((a >= 0x38) && (a <= 0x3C)) || ((a >= 0x3E) && (a <= 0x42)) || (a == 0x48) ||
           (a == 0x4A) || ((a >= 0x4C) && (a <= 0x4D)) || ((a >= 0x4F) && (a <= 0x50)) ||
           ((a >= 0x52) && (a <= 0x53)) || ((a >= 0x58) && (a <= 0x5B)) ||
           ((a >= 0x5F) && (a <= 0x65)) || (a == 0x09) || (a == 0x0C) || (a == 0x0F) ||
           (a == 0x19) || (a == 0x21) || (a == 0x25) || (a == 0x29) || (a == 0x82) ||
           ((a >= 0x89) && (a <= 0x8F) && (a != 0x8B) && (a != 0x8C)) ||
           ((a >= 0x91) && (a <= 0x92)) || ((a >= 0x94) && (a <= 0x96)) ||
           ((a >= 0x98) && (a <= 0x9E));
  }

  void writeRegister(uint8_t address, uint8_t value) noexcept {
    const bool buck_volt = (address == 0x32) || (address == 0x38);
    const uint8_t old = regs_[address];
    uint8_t next = value;
    if (readings.rw1s_set_only) {
      next = static_cast<uint8_t>(old | value);
    }
    if (buck_volt && (old != next) && ((regs_[address == 0x32 ? 0x35 : 0x3B] & 1U) != 0U)) {
      pending_dvs_ = static_cast<uint8_t>(pending_dvs_ | (address == 0x32 ? 1U : 2U));
      dvs_done_at_ = now_us_ + readings.dvs_done_delay_us;
    }
    regs_[address] = next;
  }

  void writeBatfet(uint8_t value) noexcept {
    const bool edge = (regs_[0x91] & 3U) == 0U;
    if (((value & 3U) == 1U) && (edge || !readings.wdtclr_holds)) {
      ++wdt_clears;
      env.watchdog_expired = false;
    }
    // By default WDTCLR does not hold (reads 00); with C58 it stores what was written.
    regs_[0x91] = readings.wdtclr_holds ? value : static_cast<uint8_t>(value & ~3U);
  }

  void writeGoto(uint8_t value) noexcept {
    regs_[0x5B] = value;
    const bool on = (state_ == 0x0C) || (state_ == 0x0D) || (state_ == 0x0E);
    if (((value & 1U) != 0U) && on) {
      const bool with_vbus = env.vbus_valid;
      const bool ignored = with_vbus && (readings.ship_with_vbus == ShipWithVbus::BitIgnored);
      // SHIP only with VBUS below 1 V (Table 83 J), unless the reading says it ships anyway.
      ship_entered = !with_vbus || (readings.ship_with_vbus == ShipWithVbus::ShipsAnyway);
      if (!ignored) {
        state_ = 0x2B;
      }
    }
    if (((value & 2U) != 0U) && (on || !readings.core_off_needs_run) && !env.vbus_valid &&
        !env.onkey_pushed) {
      core_off_entered = true;
      state_ = 0x2B;
    }
  }

  [[nodiscard]] uint8_t chargerState() const noexcept {
    if (!env.vbus_valid || env.vbus_ovlo) {
      return 8;
    }
    if (env.thermal_shutdown) {
      return 10;
    }
    if (env.timer_fault) {
      return 6;
    }
    if (env.battery_ov) {
      return 9;
    }
    if (env.watchdog_expired && readings.wdt_reports_0x0b) {
      return 11;
    }
    if (env.thermistor_outside && ((regs_[0x92] & 3U) != 0U)) {
      return 7;
    }
    const uint8_t mode = regs_[0x89] & 3U;
    if (mode == 0U) {
      return 8;
    }
    if (mode == 1U) {
      return 12;
    }
    if (env.battery_mv < 3000U) {
      return 0;
    }
    return (env.battery_mv < 4150U) ? 1 : 2;
  }

  void refresh() noexcept {
    const std::array<uint8_t, 8> saved = {regs_[0x06], regs_[0x26], regs_[0x2A], regs_[0x67],
                                          regs_[0x84], regs_[0x86], regs_[0x87], regs_[0x88]};
    regs_[0x67] = state_;
    uint32_t vbus = 0U;
    vbus |= env.vbus_valid ? 0x20U : 0x00U;
    vbus |= env.vbus_ovlo ? 0x10U : 0x00U;
    vbus |= env.vbus_valid ? 0x00U : 0x04U;
    vbus |= env.vbus_dpm ? 0x80U : 0x00U;
    regs_[0x86] = static_cast<uint8_t>(vbus);
    uint32_t chg = chargerState();
    chg |= env.watchdog_expired ? 0x20U : 0x00U;
    chg |= env.thermistor_outside ? 0x40U : 0x00U;
    regs_[0x87] = static_cast<uint8_t>(chg);
    uint32_t batt = 0U;
    if (env.vbus_valid) {
      if (!env.battery_present) {
        batt = 6U;
      } else if (env.battery_ov) {
        batt = 5U;
      } else {
        batt = (env.battery_mv < 3000U) ? 1U : 4U;
      }
    }
    batt |= env.battery_oc ? 0x20U : 0x00U;
    regs_[0x88] = static_cast<uint8_t>(batt);
    regs_[0x84] = static_cast<uint8_t>(vbus);
    regs_[0x26] = env.onkey_pushed ? 1U : 0U;
    regs_[0x2A] = env.pwron_high ? 0x04U : 0x00U;
    // INT_CATEGORY from the status bits
    uint32_t cat = 0U;
    cat |= (regs_[0x80] != 0U) ? 0x01U : 0U;
    for (uint32_t sw = 0U; sw < 3U; ++sw) {
      const uint32_t m = 1U << sw;
      cat |= (((regs_[0x08] & m) != 0U) || ((regs_[0x0B] & m) != 0U) || ((regs_[0x0E] & m) != 0U))
                 ? (2U << sw)
                 : 0U;
    }
    cat |= (regs_[0x18] != 0U) ? 0x10U : 0U;
    cat |= (regs_[0x24] != 0U) ? 0x20U : 0U;
    cat |= (regs_[0x20] != 0U) ? 0x40U : 0U;
    cat |= (regs_[0x28] != 0U) ? 0x80U : 0U;
    regs_[0x06] = static_cast<uint8_t>(cat);
    static constexpr std::array<uint8_t, 8> kDerived = {0x06, 0x26, 0x2A, 0x67,
                                                        0x84, 0x86, 0x87, 0x88};
    for (std::size_t i = 0; i < kDerived.size(); ++i) {
      if (pinned_[kDerived[i]]) {
        regs_[kDerived[i]] = saved[i];
      }
    }
  }

  std::array<uint8_t, 0xA0> regs_{};
  std::array<bool, 0xA0> pinned_{};
  uint8_t option_ = 2;
  uint8_t state_ = 0x0C;
  uint8_t pending_dvs_ = 0;
  uint64_t dvs_done_at_ = 0;
  uint64_t now_us_ = 0;
};

} // namespace pf1550_test

#endif // PF1550_MOCK_PF1550_HPP
