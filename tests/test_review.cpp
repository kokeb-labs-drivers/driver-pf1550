/**
 * @file test_review.cpp
 * @brief The safety review's findings, one test each: a bad read must not reach a write (B1, M1),
 * clear-on-read status bits are kept (M2), held voltages and DVS declarations are checked (M3, M4),
 * an unsafe charger can be switched off (M5), shutdown requests can be cancelled (M6), the load
 * switch needs its input voltage (M7) and a chip reset is noticed (M8).
 */
#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
Config ChargerSafeConfig() {
  Config c = BaseConfig();
  c.sw1.dvs = Dvs::Enabled; // option 2 (A2): SW1 is a DVS buck
  return c;
}

struct Ready {
  Rig rig;
  Driver driver;
  explicit Ready(const Config& c = ChargerSafeConfig()) : rig(2, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
};
} // namespace

TEST_CASE("B1 the watchdog never writes BATFET_CNFG from a read that shows protection off") {
  Ready r;
  r.rig.port.state.stuck_addr = 0x91; // a stuck line reads 0x00: BATFET_OC would read as off
  r.rig.port.state.stuck_value = 0x00;
  r.rig.Chip().writes.Clear();
  CHECK_EQ(r.driver.ServiceChargerWatchdog().error, Error::Unsafe);
  CHECK(!WroteTo(r.rig, 0x91));
  r.rig.port.state.stuck_value = 0xFF; // all ones is no register value either
  CHECK_EQ(r.driver.ServiceChargerWatchdog().error, Error::VerifyFailed);
  CHECK(!WroteTo(r.rig, 0x91));
}

TEST_CASE("B1 a single bad read is outvoted by the repeat, and the write keeps the protection") {
  Ready r;
  r.rig.port.state.flip_addr = 0x91;
  r.rig.port.state.flip_mask = 0x30; // the first read loses BATFET_OC
  r.rig.port.state.flip_count = 1;
  CHECK(r.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(r.rig.Chip().Peek(0x91) & 0x30U, 0x30U);
}

TEST_CASE("M1 a typed write whose read-back differs outside the mask fails") {
  Ready r;
  // update(): two reads (skipped), the write, then the verify read: the third read is corrupted.
  r.rig.port.state.flip_addr = 0x35;
  r.rig.port.state.flip_mask = 0x40;
  r.rig.port.state.flip_skip = 2;
  r.rig.port.state.flip_count = 1;
  CHECK_EQ(r.driver.SetRailModes(Rail::Sw1, {true, true, false, false}).error, Error::VerifyFailed);
}

TEST_CASE("M1 a bad first read of an update is not written back") {
  Ready r;
  r.rig.port.state.flip_addr = 0x35;
  r.rig.port.state.flip_mask = 0x40;
  r.rig.port.state.flip_count = 1;
  r.rig.Chip().writes.Clear();
  CHECK(r.driver.SetRailModes(Rail::Sw1, {true, true, false, false}).ok());
  CHECK_EQ(r.rig.Chip().Peek(0x35) & 0x40U, 0x00U); // the corrupted bit never reached the chip
}

TEST_CASE("M2 status bits read by ReadRegister are kept and then reported once") {
  Ready r;
  r.rig.Chip().readings.int_clear = ClearMode::Both; // the read clears them in the chip too
  r.rig.Chip().Raise(0x24, 3);                       // 3SI
  const Result<uint8_t> raw = r.driver.ReadRegister(0x24);
  REQUIRE(raw.ok());
  CHECK_EQ(raw.value & 0x08U, 0x08U);
  CHECK_EQ(r.rig.Chip().Peek(0x24), 0x00); // gone from the chip
  CHECK(Has(r.driver.GetLatchedEvents(), IrqSource::Onkey3s));
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::Onkey3s));
  CHECK(!Has(r.driver.GetLatchedEvents(), IrqSource::Onkey3s)); // reported once
  CHECK(!Has(r.driver.ServiceInterrupts().value, IrqSource::Onkey3s));
}

TEST_CASE("M2 a fault read by SelfCheck is not lost to the next service") {
  Ready r;
  r.rig.Chip().readings.int_clear = ClearMode::Both;
  r.rig.Chip().Raise(0x18, 1); // LDO2_FAULTI
  const Result<SelfCheckReport> s = r.driver.SelfCheck();
  REQUIRE(s.ok());
  CHECK(!s.value.no_latched_fault);
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::Ldo2Fault) && e.value.fault_class);
  r.driver.ClearLatchedEvents();
  CHECK(!r.driver.GetLatchedEvents().fault_class);
}

TEST_CASE("M3 a rail is not enabled at a held voltage outside its window") {
  Config c = ChargerSafeConfig();
  c.limits.rail[static_cast<std::size_t>(Rail::Ldo1)] = {1700, 1900, 100};
  Ready r(c);
  r.rig.Chip().Poke(0x4C, 15); // LDO1 holds 1.5 V
  r.rig.Chip().writes.Clear();
  CHECK_EQ(r.driver.SetRailModes(Rail::Ldo1, {true, true, false, false}).error, Error::OutOfLimits);
  CHECK(!WroteTo(r.rig, 0x4D));
  r.rig.Chip().Poke(0x4C, 16); // 1.8 V
  CHECK(r.driver.SetRailModes(Rail::Ldo1, {true, true, false, false}).ok());
  CHECK(r.driver.SetRailModes(Rail::Ldo1, {false, false, false, false}).ok()); // off: no check
}

TEST_CASE("M3 Configure checks the held voltage of a rail whose voltage it leaves alone") {
  Config c = ChargerSafeConfig();
  c.ldo1.apply = true; // volt 0: keep the held one
  c.ldo1.modes.run = true;
  c.limits.rail[static_cast<std::size_t>(Rail::Ldo1)] = {1700, 1900, 100};
  Rig rig(2, c);
  rig.Chip().Poke(0x4C, 15);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.Configure().error, Error::OutOfLimits);
  CHECK(!WroteTo(rig, 0x4D));
}

TEST_CASE("M3 the standby and sleep voltages of a DVS buck are checked as well") {
  Ready r;
  r.rig.Chip().Poke(0x33, 63); // SW1 standby voltage 1.3875 V: outside 1.0 to 1.2 V
  r.rig.Chip().writes.Clear();
  CHECK_EQ(r.driver.SetRailModes(Rail::Sw1, {true, true, false, false}).error, Error::OutOfLimits);
  CHECK(!WroteTo(r.rig, 0x35));
  CHECK(r.driver.SetRailModes(Rail::Sw1, {true, false, false, false}).ok()); // standby not used
}

TEST_CASE("M4 a DVS declaration is checked against Table 85") {
  Config c = BaseConfig();
  c.sw2.apply = true;
  c.sw2.dvs = Dvs::Enabled; // A2 starts SW2 non-DVS
  Rig a2(2, c);
  Driver d(a2.port, a2.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Refused);
  Config c1 = BaseConfig();
  c1.sw1.apply = true; // A1 starts SW1 non-DVS and SW2 in DVS: the declaration says the reverse
  c1.sw1.dvs = Dvs::Enabled;
  Rig a1(1, c1);
  c1.as_found_policy = AsFoundPolicy::MakeSafe;
  a1.cfg = c1;
  Driver d1(a1.port, a1.cfg);
  CHECK_EQ(d1.EnsureInitialized().error, Error::Refused);
}

TEST_CASE("M4 a part Table 85 does not list needs the acknowledgement for DVS") {
  Config c = BaseConfig();
  c.sw1.dvs = Dvs::Enabled;
  c.as_found_policy = AsFoundPolicy::MakeSafe;
  Rig rig(5, c);
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Unsafe);
  Config ok = c;
  ok.ack.dvs_on_unlisted_part = true;
  Rig rig2(5, ok);
  Driver d2(rig2.port, rig2.cfg);
  CHECK(d2.EnsureInitialized().ok());
  Rig a0(0, c);
  Driver d0(a0.port, a0.cfg);
  CHECK_EQ(d0.EnsureInitialized().error, Error::Unsafe); // A0: nothing is assumed either
}

TEST_CASE("M4 the part's DVS mode decides how an unconfigured buck is read") {
  Config c = BaseConfig(); // SW1 declared non-DVS, not applied; A2 holds SW1 in DVS mode
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.ReadRail(Rail::Sw1).value.volt.value, 1000000U); // A2 holds code 32: DVS table
}

TEST_CASE("M5 an unsafe charger found at start-up can be switched off, and not on") {
  Rig rig(1); // A1: charging with no timer, policy Report
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Unsafe);
  CHECK(!d.IsInitialized());
  const Result<ChargerAsFound> found = d.CheckChargerAgainstLimits();
  CHECK_EQ(found.error, Error::Unsafe);
  CHECK(found.value.mode == ChargerMode::Charge);
  CHECK_EQ(d.SetChargerMode(ChargerMode::Charge).error, Error::NotInitialized);
  CHECK(d.SetChargerMode(ChargerMode::Off).ok());
  CHECK_EQ(rig.Chip().Peek(0x89) & 0x03U, 0x00U);
  CHECK(d.CheckChargerAgainstLimits().ok());
  CHECK(d.SetRailModes(Rail::Ldo1, {}).ok()); // a switch-off still goes out
  CHECK_EQ(d.SetRailModes(Rail::Ldo1, {true, false, false, false}).error, Error::NotInitialized);
}

TEST_CASE("M5 nothing works before the chip has been identified") {
  Rig rig;
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.SetChargerMode(ChargerMode::Off).error, Error::NotInitialized);
  CHECK_EQ(d.CheckChargerAgainstLimits().error, Error::NotInitialized);
  CHECK_EQ(d.CancelShutdownRequest().error, Error::NotInitialized);
  CHECK_EQ(rig.port.state.transfers, 0);
}

TEST_CASE("M6 CancelShutdownRequest clears both command bits and verifies") {
  Ready r;
  r.rig.Chip().Poke(0x5B, 0x03);
  CHECK(r.driver.CancelShutdownRequest().ok());
  CHECK_EQ(r.rig.Chip().Peek(0x5B) & 0x03U, 0x00U);
}

TEST_CASE("M7 the load switch needs an input voltage inside the window and a rail not in LPWR") {
  Ready r;
  r.rig.Chip().writes.Clear();
  CHECK_EQ(r.driver.EnableLoadSwitch(Rail::Ldo1, LoadSwitchConfirm{.input_mv = 5000}).error,
           Error::OutOfLimits);
  CHECK_EQ(r.driver.EnableLoadSwitch(Rail::Ldo1, LoadSwitchConfirm{.input_mv = 0}).error,
           Error::OutOfLimits);
  CHECK(!WroteTo(r.rig, 0x4D));
  r.rig.Chip().Poke(0x4D, 0x0F); // low power mode set
  CHECK_EQ(r.driver.EnableLoadSwitch(Rail::Ldo1, LoadSwitchConfirm{.input_mv = 1800}).error,
           Error::Refused);
  CHECK(!WroteTo(r.rig, 0x4D));
}

namespace {
/// A change away from the start-up value (LDO1 enabled in sleep), so that a reset can be seen.
void Diverge(Ready& r) {
  CHECK(r.driver.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
}
} // namespace

TEST_CASE("M8 a chip reset in the session is noticed and the driver starts over") {
  Ready r;
  Diverge(r);
  r.rig.Chip().Reset(2); // UVDET, a watchdog or REGS_DISABLE: back to the start-up values
  r.rig.Chip().writes.Clear();
  CHECK_EQ(r.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  CHECK(!r.driver.IsInitialized());
  CHECK(!WroteTo(r.rig, 0x91)); // nothing is written after the reset
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::NotInitialized);
  CHECK(r.driver.EnsureInitialized().ok());
  CHECK(r.driver.ServiceChargerWatchdog().ok());
}

TEST_CASE("M8 every changing call checks for a reset first") {
  Ready r;
  Diverge(r);
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.driver.SetRailVoltage(Rail::Sw1, FromMillivolts(1100)).error, Error::DeviceReset);
  CHECK(r.driver.EnsureInitialized().ok());
  Diverge(r);
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.driver.Configure().error, Error::DeviceReset);
  CHECK(r.driver.EnsureInitialized().ok());
  Diverge(r);
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.driver.EnterShip(ShipConfirm{}).error, Error::DeviceReset);
  CHECK(r.driver.EnsureInitialized().ok());
  Diverge(r);
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.driver.SelfCheck().error, Error::DeviceReset);
}

TEST_CASE("F1 a reset that leaves PWRUP_I clear is still seen (the flag is not a reset flag)") {
  Ready r;
  Diverge(r);
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.rig.Chip().Peek(0x28) & 0x01U, 0x00U); // the model does not set PWRUP_I at reset
  CHECK_EQ(r.driver.SetRailModes(Rail::Ldo1, {true, true, false, false}).error, Error::DeviceReset);
}

TEST_CASE("F1 a STANDBY to RUN transition sets PWRUP_I and is no reset") {
  Ready r;
  Diverge(r);
  r.rig.Chip().Raise(0x28, 0); // PWRUP_I, as the transition raises it (Table 108)
  r.rig.Chip().writes.Clear();
  CHECK(r.driver.SetRailModes(Rail::Ldo1, {true, true, false, false}).ok());
  CHECK(r.driver.IsInitialized());
  CHECK(r.driver.ServiceChargerWatchdog().ok());
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  REQUIRE(e.ok()); // the event is an event, not an error
  CHECK(Has(e.value, IrqSource::PowerUp));
}

TEST_CASE("F1 C59: under the reading that a reset sets PWRUP_I the reset is seen as well") {
  Ready r;
  r.rig.Chip().readings.pwrup_flag_at_reset = true;
  Diverge(r);
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.rig.Chip().Peek(0x28) & 0x01U, 0x01U);
  CHECK_EQ(r.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
}

TEST_CASE("F1 a reset reached by ServiceInterrupts leaves the events in the latch") {
  Ready r;
  Diverge(r);
  r.rig.Chip().Reset(2);
  r.rig.Chip().Raise(0x24, 3);
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::Onkey3s));
  CHECK_EQ(r.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
}

TEST_CASE("m-2 starting again after a reset keeps the events in the latch") {
  Ready r;
  Diverge(r);
  r.rig.Chip().Raise(0x24, 3);
  CHECK(r.driver.ReadRegister(0x24).ok()); // noted in the latch, cleared in the chip
  r.rig.Chip().Reset(2);
  CHECK_EQ(r.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  CHECK(r.driver.EnsureInitialized().ok());
  CHECK(Has(r.driver.GetLatchedEvents(), IrqSource::Onkey3s));
}

TEST_CASE("m18 C58: if WDTCLR holds what is written, every service is still an edge") {
  Ready r;
  r.rig.Chip().readings.wdtclr_holds = true;
  CHECK(r.driver.ServiceChargerWatchdog().ok());
  CHECK(r.driver.ServiceChargerWatchdog().ok());
  CHECK(r.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(r.rig.Chip().wdt_clears, 3U);
  CHECK_EQ(r.rig.Chip().Peek(0x91) & 0x30U, 0x30U); // the protection bits are kept throughout
}

TEST_CASE("m19 a battery path found forced off at start-up is reported") {
  Rig rig(2);
  rig.Chip().Poke(0x89, 0x10); // DISBATFET set in CHG_OPER, charger off
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Unsafe);
  const Result<ChargerAsFound> found = d.CheckChargerAgainstLimits();
  CHECK(found.value.disbatfet);
  Config c = BaseConfig();
  c.ack.allow_batfet_off = true;
  Rig rig2(2, c);
  rig2.Chip().Poke(0x89, 0x10);
  Driver d2(rig2.port, rig2.cfg);
  CHECK(d2.EnsureInitialized().ok());
}

TEST_CASE("m15 a status register that clears on read cannot lose what the driver already read") {
  Ready r;
  r.rig.Chip().readings.int_clear = ClearMode::OnRead;
  r.rig.Chip().Raise(0x24, 3);
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::Onkey3s)); // returned from the read that cleared it
  CHECK_EQ(r.rig.Chip().Peek(0x24), 0x00);
  // An event of another source after that read is still pending: only the bits read are cleared.
  r.rig.Chip().Raise(0x24, 4);
  CHECK(Has(r.driver.ServiceInterrupts().value, IrqSource::Onkey4s));
}

namespace {
bool NeverWroteCommand(Rig& rig) {
  return !WroteTo(rig, 0x5B); // PWRCTRL3 is written only by the shutdown calls
}
} // namespace

TEST_CASE("m20 the typed setter never reaches a status or command register") {
  Rig rig(2, ChargerConfigOn());
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK(NeverWroteCommand(rig));
}
