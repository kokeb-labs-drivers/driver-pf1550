/**
 * @file test_round6.cpp
 * @brief The sixth review round: the input limit check of the status and interrupt paths does not
 * write after an unseen chip reset (R6-1), a failed check in `ServiceInterrupts()` is made again
 * and its error is returned (R6-2), and a status read that finds the limit right writes nothing.
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
  /// Every source unmasked, a charger interrupt (the cable) pending.
  void ChargerInterrupt() {
    rig.Chip().readings.int_clear = ClearMode::OnRead;
    REQUIRE(driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
    rig.Chip().Raise(0x80, 5);
  }
};
} // namespace

TEST_CASE("R6-1 a status read after an unseen chip reset does not write the input limit") {
  Live l;
  l.Charging();
  l.rig.Chip().Reset(2); // the OTP limit and the reset charger mode are back
  l.rig.Chip().env.vbus_valid = true;
  l.rig.Chip().writes.Clear();
  CHECK_EQ(l.driver.ReadChargerStatus().error, Error::DeviceReset);
  CHECK(!WroteTo(l.rig, kVbusLim));
}

TEST_CASE("R6-1 an interrupt service after an unseen chip reset does not write the input limit") {
  Live l;
  l.Charging();
  l.rig.Chip().Reset(2);
  l.rig.Chip().env.vbus_valid = true;
  l.rig.Chip().readings.int_clear = ClearMode::OnRead;
  l.rig.Chip().Poke(0x82, 0x00); // CHG_INT_MASK: the charger interrupt is unmasked
  l.rig.Chip().Raise(0x80, 5);
  l.rig.Chip().writes.Clear();
  CHECK_EQ(l.driver.ServiceInterrupts().error, Error::DeviceReset);
  CHECK(!WroteTo(l.rig, kVbusLim));
}

TEST_CASE("R6-2 a failed limit check in the interrupt service is made again by the next call") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  l.ChargerInterrupt();
  // INT_CATEGORY, the status, its clear and the second INT_CATEGORY pass: the check starts after.
  l.rig.port.state.fail_transfer = l.rig.port.state.transfers + 4;
  CHECK_EQ(l.driver.ServiceInterrupts().error, Error::Bus);
  l.rig.port.state.fail_transfer = -1;
  CHECK(l.rig.Chip().Peek(kVbusLim) != LimitByte(500)); // still the OTP limit
  // The interrupt is cleared and INTB is high: the next call has nothing to read but the check.
  const Result<InterruptEvents> again = l.driver.ServiceInterrupts();
  CHECK(again.ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
}

TEST_CASE("R6-2 the interrupt service returns the error of the limit check") {
  Live l(BaseConfig()); // no configured limit: an input above Limits stops the charger
  l.rig.Chip().Poke(kVbusLim, 0xA0);
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.ChargerInterrupt();
  CHECK_EQ(l.driver.ServiceInterrupts().error, Error::OutOfLimits);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

TEST_CASE("R6-4 a status read that finds the input limit right writes nothing") {
  Live l;
  l.Charging();
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.ReadChargerStatus().ok());
  CHECK(!WroteTo(l.rig, kVbusLim));
  CHECK_EQ(l.rig.Chip().writes.size(), std::size_t{0});
}

TEST_CASE("R6-5 the VBUS_SNS register decides: a restore waits when it shows VBUSIN invalid") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  l.rig.Chip().Poke(0x86, 0x53); // the register says invalid although the supply is there
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK(!WroteTo(l.rig, kVbusLim));
  CHECK(!l.driver.SelfCheck().value.input_limit_ok);
}
