/**
 * @file test_round3.cpp
 * @brief The third review round: a cable removal is not a chip reset (N1), a switch-off is sent
 * even after a failed re-start (N2), the transfer buffers belong to the driver (N3), the guards
 * that no test reached (N4), and the smaller defects (arguments checked before the chip is read, a
 * rail the driver switched off, the shutdown gates, a reset during `Configure()`).
 */
#include <algorithm>
#include <bit>
#include <cstdint>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
Config RoundConfig() {
  Config c = ChargerConfigOn();
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
  /// A reset, reported by the next call that changes the chip.
  void ResetAndReport() {
    rig.Chip().Reset(2);
    CHECK_EQ(driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  }
};

constexpr uint8_t kLdo1Ctrl = 0x4D;
constexpr uint8_t kChgOper = 0x89;
constexpr uint8_t kVbusLim = 0x94;
} // namespace

// ---- N1: an invalid VBUSIN restores the input limit only, and that is not a chip reset
// ------------

TEST_CASE("N1 a cable removal puts the input limit back to its OTP value and is not a reset") {
  Live l;
  CHECK(l.driver.Configure().ok());
  l.Diverge();
  const uint8_t configured = l.rig.Chip().Peek(kVbusLim);
  l.rig.Chip().VbusRemoved();
  CHECK(l.rig.Chip().Peek(kVbusLim) != configured); // the OTP limit differs from the written one
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, true, false, false}).ok());
  CHECK(l.driver.IsInitialized());
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK(l.driver.ServiceInterrupts().ok());
  CHECK(l.driver.ReadSnapshot().ok());
}

TEST_CASE(
    "N1 after a cable removal a limit above the supply is refused when charging is requested") {
  Live l;
  CHECK(l.driver.Configure().ok());
  l.rig.Chip().VbusRemoved();
  CHECK(l.driver.CheckChargerAgainstLimits().ok()); // the OTP limit passed the start-up check
  l.rig.Chip().Poke(kVbusLim, 0xA0);                // 1500 mA, above the 1 A supply: not covered
  CHECK_EQ(l.driver.SetChargerMode(ChargerMode::Charge).error, Error::OutOfLimits);
  CHECK((l.rig.Chip().Peek(kChgOper) & 0x03U) != 0x02U); // charging was not started
}

TEST_CASE("N1 a whole-chip reset is still seen when the cable was removed before it") {
  Live l;
  CHECK(l.driver.Configure().ok());
  l.Diverge();
  l.rig.Chip().VbusRemoved();
  l.rig.Chip().Reset(2);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
}

// ---- N2: after a reset report a failed re-start does not stop a switch-off
// --------------------------

namespace {
void FailedRestart(Live& l) {
  l.Diverge();
  l.ResetAndReport();
  l.rig.port.state.fail_from = l.rig.port.state.transfers; // the bus is down for the re-start
  CHECK(!l.driver.EnsureInitialized().ok());
  l.rig.port.state.fail_from = -1; // and back
  l.rig.Chip().writes.Clear();
}
} // namespace

TEST_CASE("N2 SetRailModes off is sent after a re-start that failed on a bus error") {
  Live l;
  FailedRestart(l);
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK(WroteTo(l.rig, kLdo1Ctrl));
  CHECK_EQ(l.rig.Chip().Peek(kLdo1Ctrl) & 0x07U, 0x00U);
}

TEST_CASE("N2 SetChargerMode(Off) is sent after a re-start that failed on a bus error") {
  Live l;
  FailedRestart(l);
  l.rig.Chip().Poke(kChgOper, 0x02);
  CHECK(l.driver.SetChargerMode(ChargerMode::Off).ok());
  CHECK(WroteTo(l.rig, kChgOper));
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

TEST_CASE("N2 CancelShutdownRequest is sent after a re-start that failed on a bus error") {
  Live l;
  FailedRestart(l);
  l.rig.Chip().Poke(0x5B, 0x30);
  CHECK(l.driver.CancelShutdownRequest().ok());
  CHECK(WroteTo(l.rig, 0x5B));
}

TEST_CASE("N2 another device answering as DEVICE_ID does stop the switch-off calls") {
  Live l;
  l.Diverge();
  l.ResetAndReport();
  l.rig.Chip().Poke(0x00, 0x55);
  CHECK_EQ(l.driver.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK_EQ(l.driver.SetRailModes(Rail::Ldo1, {}).error, Error::NotInitialized);
}

// ---- N3: the buffers the port sees are the driver's --------------------------------------------

TEST_CASE("N3 the transfer buffers are inside the driver object, not on the stack") {
  Live l;
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, false, false, false}).ok());
  const auto begin = std::bit_cast<std::uintptr_t>(&l.driver);
  const auto end = begin + sizeof(l.driver);
  const auto inside = [&](const uint8_t* p) {
    const auto at = std::bit_cast<std::uintptr_t>(p);
    return (at >= begin) && (at < end);
  };
  CHECK(inside(l.rig.port.state.seen_tx));
  CHECK(inside(l.rig.port.state.seen_rx));
}

// ---- N4: the guards no test reached ------------------------------------------------------------

TEST_CASE("N4 one flipped read of a sentinel register is not a reset") {
  Live l;
  l.Diverge();
  l.rig.port.state.flip_addr = kLdo1Ctrl;
  l.rig.port.state.flip_mask = 0xFF;
  l.rig.port.state.flip_count = 1; // the first read of the sentinel is wrong, the second is right
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK(l.driver.IsInitialized());
}

TEST_CASE("N4 a bit of a sentinel register outside the recorded bits changing is not a reset") {
  Live l;
  l.Diverge();
  l.rig.Chip().Poke(kLdo1Ctrl, static_cast<uint8_t>(l.rig.Chip().Peek(kLdo1Ctrl) ^ 0x80U));
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK(l.driver.IsInitialized());
}

TEST_CASE("N4 SetIrqMask and SetPowerDownSequence report a reset before they write") {
  Config c = RoundConfig();
  c.ack.power_down_order_reviewed = true;
  Live l(c);
  l.Diverge();
  l.rig.Chip().Reset(2);
  l.rig.Chip().writes.Clear();
  CHECK_EQ(l.driver.SetIrqMask(IrqMask{}).error, Error::DeviceReset);
  CHECK_EQ(WriteCount(l.rig), 0U);
  Live m(c);
  m.Diverge();
  m.rig.Chip().Reset(2);
  m.rig.Chip().writes.Clear();
  PowerDownSequence next{};
  next.order = {1, 2, 3, 4, 5, 6, 7};
  CHECK_EQ(m.driver.SetPowerDownSequence(next).error, Error::DeviceReset);
  CHECK_EQ(WriteCount(m.rig), 0U);
}

TEST_CASE(
    "N4 a protection that reads as off is not written back by a call that names another field") {
  // The read that decides what is written back is the one a stuck line reaches: for every position
  // of a two-read disturbance on CHG_OPER during the charge start, DISBATFET is never written set.
  for (int skip = 0; skip < 12; ++skip) {
    Live l;
    REQUIRE(l.driver.Configure().ok()); // the timer and limits are covered: charging may start
    l.rig.port.state.flip_addr = kChgOper;
    l.rig.port.state.flip_mask = 0x10; // DISBATFET
    l.rig.port.state.flip_skip = skip;
    l.rig.port.state.flip_count = 2; // two equal wrong reads
    const Error started = l.driver.SetChargerMode(ChargerMode::Charge).error;
    static_cast<void>(started);
    CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x10U, 0x00U);
  }
}

TEST_CASE("N4 a masked source with a pending event is read although INTB is high") {
  Live l;
  IrqMask mask{};
  mask.masked = {0, 0, 0, 0, 0, 0, 0, 0x02}; // PWRDN_I hidden: INTB proves nothing about it
  CHECK(l.driver.SetIrqMask(mask).ok());
  l.rig.Chip().Raise(0x28, 1);
  CHECK(!l.rig.Chip().IntbAsserted());
  const Result<InterruptEvents> e = l.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::PowerDown));
}

TEST_CASE("N4 a clear writes the bits read, so an event that arrives before it is not lost") {
  Live l;
  CHECK(l.driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  l.rig.Chip().Raise(0x28, 1);              // PWRDN_I
  l.rig.port.state.raise_after_addr = 0x28; // PWRUP_I arrives after the read, before the clear
  l.rig.port.state.raise_reg = 0x28;
  l.rig.port.state.raise_bit = 0;
  const Result<InterruptEvents> e = l.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::PowerDown));
  CHECK(Has(e.value, IrqSource::PowerUp)); // seen in the next pass, not cleared unseen
}

// ---- Minors ------------------------------------------------------------------------------------

TEST_CASE("m-1 an argument error sends nothing, not even the reset check") {
  Live l;
  l.Diverge();
  const int before = l.rig.port.state.transfers;
  CHECK_EQ(l.driver.SetRailModes(static_cast<Rail>(99), {true, false, false, false}).error,
           Error::InvalidArgument);
  CHECK_EQ(l.driver.SetRailVoltage(static_cast<Rail>(99), FromMillivolts(1800)).error,
           Error::InvalidArgument);
  CHECK_EQ(l.driver.SetRailVoltage(Rail::Ldo1, Microvolts{0}).error, Error::InvalidArgument);
  CHECK(l.driver.SetRailVoltage(Rail::VrefDdr, FromMillivolts(700)).error != Error::Ok);
  CHECK_EQ(l.rig.port.state.transfers, before);
}

TEST_CASE("m-2 a rail the driver switched off that a reset brings back on is seen") {
  Live l;
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok()); // off; nothing else was ever written
  l.rig.Chip().Reset(2);                             // OTP: LDO1 on
  CHECK(l.rig.Chip().Peek(kLdo1Ctrl) != 0);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
}

TEST_CASE("m-3 the shutdown gates are read twice: one flipped VBUS read does not open the gate") {
  Live l;
  l.rig.Chip().env.vbus_valid = true; // a supply is present: SHIP must be refused
  l.rig.Chip().SetState(0x0C);
  l.rig.port.state.flip_addr = 0x86; // VBUS_SNS
  l.rig.port.state.flip_mask = 0xFF;
  l.rig.port.state.flip_count = 1; // the first read is wrong, the second and third agree
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.EnterShip(ShipConfirm{}).error != Error::Ok);
  CHECK(!l.rig.Chip().ship_entered);
}

TEST_CASE("m-4 a reset during Configure() is reported before the charger block is written") {
  Config c = RoundConfig();
  c.ldo1.apply = true;
  c.ldo1.volt = FromMillivolts(1800);
  c.ldo1.modes = {true, false, true, false}; // not the start-up modes: a reset can be seen
  bool seen = false;
  for (int k = 1; k < 80; ++k) {
    Live l(c);
    l.rig.port.state.reset_at = l.rig.port.state.transfers + k;
    const Error e = l.driver.Configure().error;
    if (e == Error::DeviceReset) {
      seen = true;
      CHECK(!WroteTo(l.rig, 0x8E)); // CHG_CURR_CNFG: the charger block was not written
    }
  }
  CHECK(seen);
}
