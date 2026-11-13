/**
 * @file test_round5.cpp
 * @brief The fifth review round: the guards of the cable-removal check that survived (a limit
 * inside Limits that is not the configured one, a refused charge start with an unacknowledged
 * protection off, single reads), the check without the watchdog, a VBUSIN that stays invalid, the
 * order of the checks, LinearOnly, a stuck DEVICE_ID line and the error names.
 */
#include <cstdint>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
constexpr uint8_t kChgOper = 0x89;
constexpr uint8_t kVbusLim = 0x94;
constexpr uint8_t kBatfet = 0x91;

constexpr uint8_t LimitByte(uint32_t ma) {
  return static_cast<uint8_t>(InputLimitCode(ma).value << 3U);
}

/// A charger block with a configured input limit of 500 mA (Limits allow 1000 mA).
Config Limit500Config(ChargerMode mode = ChargerMode::LinearOnly) {
  Config c = ChargerConfigOn();
  c.charger.mode = mode;
  c.charger.input_limit_ma = 500;
  return c;
}

struct Live {
  Rig rig;
  Driver driver;
  explicit Live(const Config& c = Limit500Config(), uint8_t option = 2)
      : rig(option, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
  /// Configured and charging, the supply valid.
  void Charging() {
    REQUIRE(driver.Configure().ok());
    REQUIRE(driver.SetChargerMode(ChargerMode::Charge).ok());
    rig.Chip().env.vbus_valid = true;
  }
};
} // namespace

// ---- R5-1: the guards that survived ------------------------------------------------------------

TEST_CASE("R5-1 a limit inside Limits that is not the configured one is written again") {
  Live l;
  l.Charging();
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
  l.rig.Chip().Poke(kVbusLim, LimitByte(1000)); // allowed by Limits, but not what was configured
  const Result<SelfCheckReport> before = l.driver.SelfCheck();
  REQUIRE(before.ok());
  CHECK(!before.value.input_limit_ok);
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x02U);
  CHECK(l.driver.SelfCheck().value.input_limit_ok);
}

TEST_CASE("R5-1 a charger found charging with an unacknowledged protection off is not started") {
  Live l(BaseConfig());
  l.rig.Chip().Poke(kVbusLim, LimitByte(400)); // inside Limits
  l.rig.Chip().Poke(kChgOper, 0x02);           // charging, the fast-charge timer is off
  CHECK_EQ(l.driver.SetChargerMode(ChargerMode::Charge).error, Error::Unsafe);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

TEST_CASE("R5-1 SelfCheck reads the input limit twice: one flipped read is outvoted") {
  Live l;
  l.Charging();
  l.rig.port.state.flip_addr = kVbusLim;
  l.rig.port.state.flip_mask = 0xF8; // the wrong read shows another limit
  l.rig.port.state.flip_count = 1;
  const Result<SelfCheckReport> r = l.driver.SelfCheck();
  REQUIRE(r.ok());
  CHECK(r.value.input_limit_ok);
}

TEST_CASE("R5-1 one flipped limit read does not cause a write") {
  Live l;
  l.Charging();
  l.rig.Chip().writes.Clear();
  l.rig.port.state.flip_addr = kVbusLim;
  l.rig.port.state.flip_mask = 0xF8;
  l.rig.port.state.flip_count = 1;
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK(!WroteTo(l.rig, kVbusLim));
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x02U);
}

TEST_CASE("R5-1 one flipped CHG_OPER read does not skip the limit check") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved(); // the OTP limit is back, the charger still charges
  l.rig.port.state.flip_addr = kChgOper;
  l.rig.port.state.flip_mask = 0x02; // the wrong read shows the charger off
  l.rig.port.state.flip_count = 1;
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
}

TEST_CASE("R5-1 a restore that the chip does not keep is reported") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  l.rig.port.state.stuck_addr = kVbusLim; // reads keep showing the OTP limit
  l.rig.port.state.stuck_value = l.rig.Chip().Peek(kVbusLim);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::VerifyFailed);
}

TEST_CASE("R5-1 EnterShip writes PWRCTRL3 from two agreeing reads") {
  Live l(BaseConfig());
  l.rig.Chip().SetState(0x0C);
  l.rig.Chip().Poke(0x5B, 0x00);
  l.rig.port.state.flip_addr = 0x5B;
  l.rig.port.state.flip_mask = 0x40; // the first read carries a bit that is not there
  l.rig.port.state.flip_count = 1;
  REQUIRE(l.driver.EnterShip(ShipConfirm{}).ok());
  CHECK(LastWriteWas(l.rig, 0x5B, 0x01));
}

// ---- R5-2: the check does not need the watchdog ------------------------------------------------

TEST_CASE("R5-2 with the watchdog off a status read finds the reverted input limit") {
  Config c = Limit500Config();
  REQUIRE(!c.charger.watchdog.enable);
  Live l(c);
  l.Charging();
  l.rig.Chip().VbusRemoved();
  CHECK(l.rig.Chip().Peek(kVbusLim) != LimitByte(500));
  CHECK(l.driver.ReadChargerStatus().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x02U);
}

TEST_CASE("R5-2 with the watchdog off an interrupt service finds the reverted input limit") {
  Live l;
  l.Charging();
  l.rig.Chip().readings.int_clear = ClearMode::OnRead;
  REQUIRE(l.driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  l.rig.Chip().VbusRemoved();
  l.rig.Chip().Raise(0x80, 5); // a charger interrupt (the cable)
  const Result<InterruptEvents> e = l.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
}

TEST_CASE(
    "R5-2 a limit above Limits that cannot be restored makes the status read fail and stops") {
  Live l(BaseConfig());
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.rig.Chip().Poke(kVbusLim, 0xA0); // 1500 mA
  CHECK_EQ(l.driver.ReadChargerStatus().error, Error::OutOfLimits);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

// ---- m-1: VBUSIN stays invalid -----------------------------------------------------------------

TEST_CASE("m-1 while VBUSIN is invalid the restore waits and nothing fails") {
  Live l;
  l.Charging();
  l.rig.Chip().env.vbus_valid = false;
  l.rig.Chip().env.vbus_limit_held = true;
  l.rig.Chip().VbusRemoved();
  l.rig.Chip().writes.Clear();
  for (int i = 0; i < 3; ++i) {
    CHECK(l.driver.ServiceChargerWatchdog().ok()); // not VerifyFailed on every call
  }
  CHECK(!WroteTo(l.rig, kVbusLim));
  CHECK(!l.driver.SelfCheck().value.input_limit_ok);
  l.rig.Chip().env.vbus_valid = true; // the supply comes back
  l.rig.Chip().env.vbus_limit_held = false;
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
}

// ---- m-2, m-3 ----------------------------------------------------------------------------------

TEST_CASE("m-2 a BATFET_CNFG read that is refused does not skip the limit check") {
  Live l;
  l.Charging();
  l.rig.Chip().VbusRemoved();
  // The protection reads as off (a stuck line), the other bits as written: refused, no reset seen.
  l.rig.port.state.stuck_addr = kBatfet;
  l.rig.port.state.stuck_value = static_cast<uint8_t>(l.rig.Chip().Peek(kBatfet) & ~0x30U);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::Unsafe);
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
}

TEST_CASE("m-3 a configured limit is kept with the linear regulator alone") {
  Live l(Limit500Config(ChargerMode::LinearOnly));
  REQUIRE(l.driver.Configure().ok());
  l.rig.Chip().env.vbus_valid = true;
  l.rig.Chip().VbusRemoved();
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x01U);
}

TEST_CASE("m-3 without a configured limit the part's own LinearOnly state is left alone") {
  Live l(BaseConfig());
  l.rig.Chip().Poke(kChgOper, 0x01);
  l.rig.Chip().Poke(kVbusLim, 0xA0); // the OTP limit of the part, 1500 mA
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK(l.driver.ReadChargerStatus().ok());
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x01U);
}

// ---- m-5: a stuck DEVICE_ID line ---------------------------------------------------------------

TEST_CASE("m-5 a DEVICE_ID line stuck at zero does not take the switch-off away") {
  Live l(BaseConfig());
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
  l.rig.Chip().Reset(2);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  l.rig.port.state.stuck_addr = 0x00;
  l.rig.port.state.stuck_value = 0x00;
  CHECK_EQ(l.driver.EnsureInitialized().error, Error::Bus);
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK(l.driver.SetChargerMode(ChargerMode::Off).ok());
  CHECK(WroteTo(l.rig, 0x4D));
  CHECK(WroteTo(l.rig, kChgOper));
}

TEST_CASE("m-5 another device on the address still takes the switch-off away") {
  Live l(BaseConfig());
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
  l.rig.Chip().Reset(2);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  l.rig.port.state.stuck_addr = 0x00;
  l.rig.port.state.stuck_value = 0x42;
  CHECK_EQ(l.driver.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK(!l.driver.SetChargerMode(ChargerMode::Off).ok());
}
