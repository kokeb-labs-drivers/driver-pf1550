/**
 * @file test_round2.cpp
 * @brief The second review round, one test per finding: a switch-off is never refused (F2), the
 * two-agreeing-reads rule and the reset checks hold on every call (F3), the safety tests run under
 * every reading of the ambiguities (F5), and the smaller defects (m-1, m-5).
 */
#include <algorithm>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
Config RoundConfig() {
  Config c = BaseConfig();
  c.sw1.dvs = Dvs::Enabled; // option 2 (A2): SW1 is a DVS buck
  return c;
}

struct Live {
  Rig rig;
  Driver driver;
  explicit Live(const Config& c = RoundConfig(), uint8_t option = 2)
      : rig(option, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
  /// A change away from the start-up value (LDO1 enabled in sleep), so that a reset can be seen.
  void Diverge() {
    CHECK(driver.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
  }
};

constexpr uint8_t kLdo1Ctrl = 0x4D;
constexpr uint8_t kChgOper = 0x89;
} // namespace

// ---- F2: a switch-off is never refused -------------------------------------------------------

TEST_CASE("F2 a rail is switched off after a reset report and after a failed start") {
  Live l;
  l.Diverge();
  l.rig.Chip().Reset(2);
  l.rig.Chip().writes.Clear();
  CHECK_EQ(l.driver.SetRailModes(Rail::Ldo1, {true, true, false, false}).error, Error::DeviceReset);
  CHECK(!l.driver.IsInitialized());
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok()); // off: identified is enough
  CHECK(WroteTo(l.rig, kLdo1Ctrl));
  CHECK_EQ(l.rig.Chip().Peek(kLdo1Ctrl) & 0x07U, 0x00U);
  Rig bad(1); // A1: charging with no timer, policy Report: the start fails
  Driver d(bad.port, bad.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Unsafe);
  bad.Chip().writes.Clear();
  CHECK(d.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK_EQ(bad.Chip().Peek(kLdo1Ctrl) & 0x07U, 0x00U);
  CHECK_EQ(d.SetRailModes(Rail::Ldo1, {true, false, false, false}).error, Error::NotInitialized);
}

TEST_CASE("F2 a rail is switched off with a line stuck at all ones, with no read to trust") {
  Live l;
  l.rig.port.state.stuck_addr = kLdo1Ctrl;
  l.rig.port.state.stuck_value = 0xFF;
  l.rig.Chip().writes.Clear();
  const Error e = l.driver.SetRailModes(Rail::Ldo1, {}).error; // the read-back is stuck too
  CHECK(e == Error::VerifyFailed);
  CHECK(WroteTo(l.rig, kLdo1Ctrl)); // but the switch-off went out
  CHECK_EQ(l.rig.Chip().Peek(kLdo1Ctrl) & 0x07U, 0x00U);
}

TEST_CASE("F2 a rail is switched off when its reads disagree") {
  Live l;
  l.rig.port.state.flip_addr = kLdo1Ctrl;
  l.rig.port.state.flip_mask = 0x08;
  l.rig.port.state.flip_skip = 1; // the first read is clean, the second is not, the third is clean
  l.rig.port.state.flip_count = 1;
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK_EQ(l.rig.Chip().Peek(kLdo1Ctrl) & 0x07U, 0x00U);
}

TEST_CASE("F2 the charger is switched off with a stuck line and after a reset") {
  Live l;
  l.rig.Chip().Poke(kChgOper, 0x02); // charging
  l.rig.port.state.stuck_addr = kChgOper;
  l.rig.port.state.stuck_value = 0xFF;
  CHECK(l.driver.SetChargerMode(ChargerMode::Off).error == Error::VerifyFailed);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U); // the off write went out
  l.rig.port.state.stuck_addr = 0x100U;
  l.Diverge();
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.rig.Chip().Reset(2);
  l.rig.Chip().Poke(kChgOper, 0x02);
  CHECK(l.driver.SetChargerMode(ChargerMode::Off).ok()); // no reset check before an off
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

TEST_CASE("F2 RestartCharger switches off first, then reports a reset") {
  Live l;
  l.Diverge();
  l.rig.Chip().Reset(2);
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.rig.Chip().writes.Clear();
  const Error e = l.driver.RestartCharger().error;
  CHECK(e == Error::DeviceReset); // the charge step needs a live driver
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
  CHECK(WroteTo(l.rig, kChgOper));
}

TEST_CASE(
    "F2 an off request keeps the protections: a read that shows one off is not written back") {
  Live l;
  l.rig.port.state.stuck_addr = kChgOper;
  l.rig.port.state.stuck_value = 0x12; // reads as charging with DISBATFET set
  CHECK(l.driver.SetChargerMode(ChargerMode::Off).error == Error::VerifyFailed);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x10U, 0x00U); // DISBATFET was not carried over
}

// ---- F3: one test per surviving mutant ---------------------------------------------------------

TEST_CASE("F3 the second read being the bad one is not believed, and nothing is written") {
  Live l;
  l.rig.port.state.flip_addr = 0x35;
  l.rig.port.state.flip_mask = 0x08;
  l.rig.port.state.flip_skip = 1; // the first read is good, the second is flipped
  l.rig.port.state.flip_count = 1;
  l.rig.Chip().writes.Clear();
  // r1 good, r2 flipped (differs), r3 good: r2 and r3 differ, so the call fails and writes nothing
  CHECK_EQ(l.driver.SetRailModes(Rail::Sw1, {true, true, false, false}).error, Error::VerifyFailed);
  CHECK(!WroteTo(l.rig, 0x35));
}

TEST_CASE("F3 a first bad read is outvoted by two good ones (three reads at most)") {
  Live l;
  l.rig.port.state.flip_addr = 0x35;
  l.rig.port.state.flip_mask = 0x08;
  l.rig.port.state.flip_count = 1; // r1 flipped, r2 and r3 good: r2 == r3
  CHECK(l.driver.SetRailModes(Rail::Sw1, {true, true, false, false}).ok());
  CHECK_EQ(l.rig.Chip().Peek(0x35) & 0x08U, 0x00U);
}

TEST_CASE("F3 CancelShutdownRequest works after a reset report and keeps the other bits") {
  Live l;
  l.Diverge();
  l.rig.Chip().Reset(2);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  l.rig.Chip().Poke(0x5B, 0xF3); // both command bits armed, other bits set
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.CancelShutdownRequest().ok());
  CHECK_EQ(l.rig.Chip().Peek(0x5B), 0xF0);
}

TEST_CASE("F3 every call that changes the chip checks for a reset first") {
  Live l;
  const auto reset = [&l]() {
    l.Diverge();
    l.rig.Chip().Reset(2);
  };
  reset();
  CHECK_EQ(l.driver.SetRailModes(Rail::Ldo2, {true, false, false, false}).error,
           Error::DeviceReset);
  CHECK(l.driver.EnsureInitialized().ok());
  reset();
  CHECK_EQ(l.driver.SetChargerMode(ChargerMode::Charge).error, Error::DeviceReset);
  CHECK(l.driver.EnsureInitialized().ok());
  reset();
  CHECK_EQ(l.driver.RestartCharger().error, Error::DeviceReset);
  CHECK(l.driver.EnsureInitialized().ok());
  reset();
  CHECK_EQ(l.driver.EnableLoadSwitch(Rail::Ldo1, LoadSwitchConfirm{.input_mv = 1800}).error,
           Error::DeviceReset);
  CHECK(l.driver.EnsureInitialized().ok());
  reset();
  CHECK_EQ(l.driver.EnterCoreOff(CoreOffConfirm{}).error, Error::DeviceReset);
}

TEST_CASE("F3 a rail is switched on only by a live driver") {
  Rig rig(1); // A1: the start fails, nothing is on
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Unsafe);
  CHECK_EQ(d.SetChargerMode(ChargerMode::Charge).error, Error::NotInitialized);
  CHECK_EQ(d.RestartCharger().error, Error::NotInitialized);
}

TEST_CASE("F3 the DVS-done wait keeps the other rail's event for the caller (m-5: not its own)") {
  Config c = RoundConfig();
  c.limits.rail[static_cast<std::size_t>(Rail::Sw1)] = {900, 1300, 800};
  Live l(c);
  l.rig.Chip().Raise(0x0E, 1); // SW2 DVS done, pending while SW1 steps
  CHECK(l.driver.SetRailVoltage(Rail::Sw1, FromMillivolts(1100)).ok());
  CHECK(Has(l.driver.GetLatchedEvents(), IrqSource::Sw2DvsDone));
  CHECK(!Has(l.driver.GetLatchedEvents(), IrqSource::Sw1DvsDone)); // consumed by the wait
}

TEST_CASE("F3 Configure waits for the DVS step of a changed SW1 voltage") {
  Config c = RoundConfig();
  c.sw1.apply = true;
  c.sw1.modes.run = true;
  c.sw1.run_uv = FromMillivolts(1100);
  Live l(c);
  l.rig.Chip().readings.dvs_never_done = true;
  CHECK_EQ(l.driver.Configure().error, Error::Timeout);
  l.rig.Chip().readings.dvs_never_done = false;
  l.rig.Chip().Reset(2);
  CHECK_EQ(l.driver.Configure().error, Error::DeviceReset); // the first attempt changed registers
  CHECK(l.driver.EnsureInitialized().ok());
  const Error again = l.driver.Configure().error;
  CHECK_EQ(again, Error::Ok);
  CHECK_EQ(l.driver.ReadRail(Rail::Sw1).value.volt.value, 1100000U);
}

TEST_CASE("F3 a Configure of SW3 leaves the unused bit 5 as the chip holds it") {
  Config c = RoundConfig();
  c.sw3.apply = true;
  c.sw3.modes.run = true;
  c.sw3.modes.standby = true;
  Live l(c);
  l.rig.Chip().Poke(0x41, 0x23); // EN, STBY_EN and the unused bit 5
  CHECK_EQ(l.driver.Configure().error, Error::Ok);
  CHECK_EQ(l.rig.Chip().Peek(0x41) & 0x20U, 0x20U);
}

TEST_CASE("F3 DISBATFET in the as-found verdict applies when charging is requested") {
  Config c = ChargerConfigOn();
  c.sw1.dvs = Dvs::Enabled;
  Live l(c);
  REQUIRE(l.driver.Configure().ok());
  l.rig.Chip().Poke(kChgOper, 0x11); // battery FET forced off, linear regulator only
  const Result<ChargerAsFound> found = l.driver.CheckChargerAgainstLimits();
  CHECK_EQ(found.error, Error::Unsafe);
  CHECK(found.value.disbatfet);
  CHECK_EQ(l.driver.SetChargerMode(ChargerMode::Charge).error, Error::Unsafe);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x01U);
  l.rig.Chip().Poke(kChgOper, 0x12); // charging with the battery FET forced off
  CHECK_EQ(l.driver.CheckChargerAgainstLimits().error, Error::Unsafe);
  l.rig.Chip().Poke(kChgOper, 0x02);
  CHECK_EQ(l.driver.CheckChargerAgainstLimits().error, Error::Ok);
  c.ack.allow_batfet_off = true;
  Live l2(c);
  REQUIRE(l2.driver.Configure().ok());
  l2.rig.Chip().Poke(kChgOper, 0x11);
  CHECK(l2.driver.SetChargerMode(ChargerMode::Charge).ok());
}

// ---- m-1: SW3's held voltage does not depend on SW2's DVS flag ----------------------------------

TEST_CASE("m-1 SW3 standby voltage is checked against Limits whatever SW2's mode") {
  for (uint8_t option = 1U; option <= 2U; ++option) { // A1: SW2 non-DVS, A2: SW2 DVS
    Config c = BaseConfig();
    c.sw1.dvs = (option == 1U) ? Dvs::Disabled : Dvs::Enabled;
    c.limits.rail[static_cast<std::size_t>(Rail::Sw3)] = {1800, 1900, 500};
    Rig rig(option, c);
    rig.Chip().Poke(0x3E, 0x0F); // a code far from the window
    Driver d(rig.port, rig.cfg);
    REQUIRE((d.EnsureInitialized().ok() || (d.EnsureInitialized().error == Error::Unsafe)));
    CHECK_EQ(d.SetRailModes(Rail::Sw3, {true, true, false, false}).error,
             (d.IsInitialized() ? Error::OutOfLimits : Error::NotInitialized));
  }
}

// ---- F5: the safety tests run under every reading of the ambiguities ----------------------------

namespace {
bool NeverBothCommands(Rig& rig) {
  return std::ranges::all_of(rig.Chip().writes, [](const WriteRecord& w) {
    return (w.address != 0x5B) || ((w.value & 0x03U) != 0x03U);
  });
}

/// The same session under one reading: start, configure, service the watchdog, request charging,
/// ask for both shutdowns, cancel, switch everything off. Whatever the chip does, the driver's
/// promises hold.
void RunUnder(const Readings& readings, bool vbus) {
  Config c = ChargerConfigOn();
  c.sw1.dvs = Dvs::Enabled;
  Rig rig(2, c);
  rig.Chip().readings = readings;
  rig.Chip().Reset(2);
  rig.Chip().env.vbus_valid = vbus;
  Driver d(rig.port, rig.cfg);
  CHECK(d.EnsureInitialized().ok());
  static_cast<void>(d.Configure());
  for (int i = 0; i < 3; ++i) {
    static_cast<void>(d.ServiceChargerWatchdog());
  }
  static_cast<void>(d.SetChargerMode(ChargerMode::Charge));
  const Result<void> ship = d.EnterShip(ShipConfirm{});
  if (vbus) {
    CHECK_EQ(ship.error, Error::Refused); // never with a supply present, whatever the chip does
    CHECK(!rig.Chip().ship_entered);
  }
  static_cast<void>(d.EnterCoreOff(CoreOffConfirm{}));
  static_cast<void>(d.CancelShutdownRequest());
  const Error charger_off = d.SetChargerMode(ChargerMode::Off).error;
  const Error rail_off = d.SetRailModes(Rail::Ldo1, {}).error;
  CHECK_EQ(rig.Chip().protected_writes, 0U);
  CHECK(NeverBothCommands(rig));
  CHECK_EQ(rig.Chip().Peek(0x91) & 0x30U, 0x30U);                 // BATFET_OC never switched off
  if (!rig.Chip().ship_entered && !rig.Chip().core_off_entered) { // else the chip is off the bus
    // The charger and the rail end off, or the call says it could not (RW1S bits that do not
    // clear).
    CHECK(((rig.Chip().Peek(0x89) & 0x03U) == 0x00U) || (charger_off != Error::Ok));
    CHECK(((rig.Chip().Peek(kLdo1Ctrl) & 0x07U) == 0x00U) || (rail_off != Error::Ok));
  }
}
} // namespace

TEST_CASE("F5 the session's promises hold under every reading of the knobs") {
  for (int pass = 0; pass < 2; ++pass) {
    const bool vbus = (pass == 1);
    RunUnder(Readings{}, vbus);
    Readings r{};
    r.int_clear = ClearMode::OnRead;
    RunUnder(r, vbus);
    r = Readings{};
    r.int_clear = ClearMode::Both;
    RunUnder(r, vbus);
    r = Readings{};
    r.sw3_write_ignored = false;
    RunUnder(r, vbus);
    r = Readings{};
    r.rw1s_set_only = true;
    RunUnder(r, vbus);
    r = Readings{};
    r.wd_time_one_is_32s = false;
    RunUnder(r, vbus);
    r = Readings{};
    r.wdt_reports_0x0b = true;
    RunUnder(r, vbus);
    r = Readings{};
    r.silicon_rev_reset = 0x08;
    RunUnder(r, vbus);
    r = Readings{};
    r.nondvs_write_accepted = false;
    RunUnder(r, vbus);
    r = Readings{};
    r.core_off_needs_run = false;
    RunUnder(r, vbus);
    r = Readings{};
    r.dvs_done_delay_us = 3000;
    RunUnder(r, vbus);
    r = Readings{};
    r.dvs_never_done = true;
    RunUnder(r, vbus);
    r = Readings{};
    r.apply_delay_us = 2000;
    RunUnder(r, vbus);
    r = Readings{};
    r.seq0_starts_first = false;
    RunUnder(r, vbus);
    r = Readings{};
    r.seq_order_zero_last = false;
    RunUnder(r, vbus);
    r = Readings{};
    r.ls_fault_interrupt = true;
    RunUnder(r, vbus);
    r = Readings{};
    r.wdtclr_holds = true;
    RunUnder(r, vbus);
    r = Readings{};
    r.pwrup_flag_at_reset = true;
    RunUnder(r, vbus);
    r = Readings{};
    r.ship_with_vbus = ShipWithVbus::BitIgnored;
    RunUnder(r, vbus);
    r = Readings{};
    r.ship_with_vbus = ShipWithVbus::ShipsAnyway;
    RunUnder(r, vbus);
  }
}

// ---- Found by the fuzzer: the option read at start-up decides how every buck voltage is read
// -----

TEST_CASE("F3 one bad read of OTP_FLAVOR does not change how the buck voltages are read") {
  for (unsigned n = 0; n < 64U; ++n) {
    const int skip = static_cast<int>(n % 4U);
    Config c = RoundConfig();
    Rig rig(2, c);
    rig.port.state.flip_addr = 0x01; // OTP_FLAVOR
    rig.port.state.flip_mask = static_cast<uint8_t>(1U + (n / 4U));
    rig.port.state.flip_skip = skip;
    rig.port.state.flip_count = 1;
    Driver d(rig.port, rig.cfg);
    if (d.EnsureInitialized().ok()) {
      const Result<RailReadout> r = d.ReadRail(Rail::Sw1);
      REQUIRE(r.ok());
      CHECK_EQ(r.value.volt.value, detail::VoltUv(Rail::Sw1, rig.Chip().Peek(0x32), true));
    }
  }
}
