/**
 * @file test_round7.cpp
 * @brief The seventh review round: a pending input limit check survives `ClearLatchedEvents()`
 * (R7-1), and the guards of the interrupt service and the switch-off paths that no test pinned
 * (R7-2): a bus error while collecting, a port without `GpioGet()`, a switch-off after an unseen
 * reset, a failed verification read of a switch-off, the pass bound, the charger-group trigger and
 * the pending flag being cleared.
 */
#include <cstdint>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
constexpr uint8_t kChgOper = 0x89;
constexpr uint8_t kVbusLim = 0x94;

constexpr uint8_t LimitByte(uint32_t ma) {
  return static_cast<uint8_t>(InputLimitCode(ma).value << 3U);
}

Config Limit500Config() {
  Config c = ChargerConfigOn();
  c.charger.mode = ChargerMode::LinearOnly;
  c.charger.input_limit_ma = 500;
  return c;
}

struct Live {
  Rig rig;
  Driver driver;
  explicit Live(const Config& c = Limit500Config()) : rig(2, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
  void Charging() {
    REQUIRE(driver.Configure().ok());
    REQUIRE(driver.SetChargerMode(ChargerMode::Charge).ok());
    rig.Chip().env.vbus_valid = true;
  }
  void Unmask() {
    rig.Chip().readings.int_clear = ClearMode::OnRead;
    REQUIRE(driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  }
};
} // namespace

// ---- R7-1 --------------------------------------------------------------------------------------

TEST_CASE("R7-1 clearing the latch does not drop a pending input limit check") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  l.Unmask();
  l.rig.Chip().Raise(0x80, 5);
  l.rig.port.state.fail_transfer = l.rig.port.state.transfers + 4; // inside the limit check
  CHECK_EQ(l.driver.ServiceInterrupts().error, Error::Bus);
  l.rig.port.state.fail_transfer = -1;
  l.driver.ClearLatchedEvents();
  CHECK(l.driver.ServiceInterrupts().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
}

TEST_CASE("R7-1 a check that passed is not made again: an idle service has no bus traffic") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  l.Unmask();
  l.rig.Chip().Raise(0x80, 5);
  CHECK(l.driver.ServiceInterrupts().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
  const int before = l.rig.port.state.transfers;
  CHECK(l.driver.ServiceInterrupts().ok());
  CHECK_EQ(l.rig.port.state.transfers, before);
}

// ---- R7-2 --------------------------------------------------------------------------------------

TEST_CASE("R7-2 a bus error while the interrupts are collected is returned") {
  for (int k = 0; k < 2; ++k) { // INT_CATEGORY, then the status byte
    Live l;
    l.Unmask();
    l.rig.Chip().Raise(0x28, 1);
    l.rig.port.state.fail_transfer = l.rig.port.state.transfers + k;
    CHECK_EQ(l.driver.ServiceInterrupts().error, Error::Bus);
  }
}

TEST_CASE("R7-2 a port without GpioGet() is read on every service") {
  MinimalPort port;
  port.state.chip.Reset(2);
  PF1550<MinimalPort> d(port, BaseConfig());
  REQUIRE(d.EnsureInitialized().ok());
  port.state.chip.readings.int_clear = ClearMode::OnRead;
  REQUIRE(d.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  port.state.chip.Raise(0x28, 1);
  const Result<InterruptEvents> e = d.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::PowerDown));
}

TEST_CASE("R7-2 a switch-off after an unseen reset is not refused") {
  Live l(BaseConfig()); // no configured limit: an input above Limits stops the charger
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
  l.rig.Chip().Reset(2); // the driver has not noticed
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.rig.Chip().Poke(kVbusLim, 0xA0); // 1500 mA, above Limits
  CHECK_EQ(l.driver.ReadChargerStatus().error, Error::OutOfLimits);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

TEST_CASE("R7-2 a switch-off whose verification read fails says so") {
  Live l(BaseConfig());
  l.rig.Chip().Poke(kChgOper, 0x02);
  // CHG_OPER is read twice (two agreeing reads), written, then read back: the fourth transfer.
  l.rig.port.state.fail_transfer = l.rig.port.state.transfers + 3;
  CHECK_EQ(l.driver.SetChargerMode(ChargerMode::Off).error, Error::Bus);
}

TEST_CASE("R7-2 at most three passes are made, and the count is reported") {
  Live l;
  l.Unmask();
  l.rig.Chip().Raise(0x28, 1);
  const Result<InterruptEvents> one = l.driver.ServiceInterrupts();
  REQUIRE(one.ok());
  CHECK_EQ(one.value.passes, 1U);
  // A category that never clears (a stuck line): every pass reads it again, three are made.
  Live m;
  m.Unmask();
  m.rig.Chip().Raise(0x28, 1); // INTB is low, so the service does not return at once
  m.rig.port.state.stuck_addr = reg::kIntCategory;
  m.rig.port.state.stuck_value = 0x01;
  const int before = m.rig.port.state.transfers;
  const Result<InterruptEvents> three = m.driver.ServiceInterrupts();
  REQUIRE(three.ok());
  CHECK_EQ(three.value.passes, 3U);
  // Three passes of INT_CATEGORY and CHG_INT, and the limit check after them: no more passes.
  CHECK(m.rig.port.state.transfers - before >= 6);
}

TEST_CASE("R7-2 only a charger interrupt starts the input limit check") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  l.Unmask();
  l.rig.Chip().Raise(0x28, 1); // a power interrupt, not the charger
  CHECK(l.driver.ServiceInterrupts().ok());
  CHECK(l.rig.Chip().Peek(kVbusLim) != LimitByte(500)); // not looked at
}
