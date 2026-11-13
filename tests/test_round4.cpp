/**
 * @file test_round4.cpp
 * @brief The fourth review round: a cable removal while charging is reported or put right (R4-1),
 * the guards that survived (R4-2: write buffer, DVS-declaration start, charge-current limit, the
 * INTB shortcut, the shutdown and cancel reads), and one wrong DEVICE_ID read (R4-3).
 */
#include <algorithm>
#include <bit>
#include <cstdint>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
constexpr uint8_t kChgOper = 0x89;
constexpr uint8_t kVbusLim = 0x94;
constexpr uint8_t kOtpLimit1500 = 0xA0; // code 20 << 3

Config ChargingConfig() {
  Config c = ChargerConfigOn();
  c.charger.mode = ChargerMode::LinearOnly;
  return c;
}

struct Live {
  Rig rig;
  Driver driver;
  explicit Live(const Config& c = ChargingConfig(), uint8_t option = 2)
      : rig(option, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
};
} // namespace

// ---- R4-1: a cable removal while charging ------------------------------------------------------

TEST_CASE("R4-1 a cable removal while charging: the configured input limit is written again") {
  Live l;
  REQUIRE(l.driver.Configure().ok());
  const uint8_t configured = l.rig.Chip().Peek(kVbusLim);
  REQUIRE(l.driver.SetChargerMode(ChargerMode::Charge).ok());
  l.rig.Chip().VbusRemoved();
  l.rig.Chip().env.vbus_valid = true; // the supply is back: the limit field is no longer held
  CHECK(l.rig.Chip().Peek(kVbusLim) != configured); // the OTP limit is back
  CHECK(l.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(l.rig.Chip().Peek(kVbusLim), configured); // and the watchdog service put it right
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U,
           0x02U); // still charging, now at the configured limit
}

TEST_CASE(
    "R4-1 a limit above Limits that cannot be restored switches the charger off and says so") {
  Live l(BaseConfig()); // the charger block is not applied: there is no configured limit
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.rig.Chip().Poke(kVbusLim, kOtpLimit1500); // 1500 mA, the supply is 1000 mA
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::OutOfLimits);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

TEST_CASE("R4-1 SelfCheck reports a limit the supply does not tolerate") {
  Live l;
  REQUIRE(l.driver.Configure().ok());
  CHECK(l.driver.SelfCheck().value.input_limit_ok);
  l.rig.Chip().VbusRemoved();
  const Result<SelfCheckReport> r = l.driver.SelfCheck();
  REQUIRE(r.ok());
  CHECK(!r.value.input_limit_ok);
}

TEST_CASE("R4-1 a refused charge start leaves the charger off") {
  Live l(BaseConfig());
  l.rig.Chip().Poke(kChgOper, 0x02);
  l.rig.Chip().Poke(kVbusLim, kOtpLimit1500);
  CHECK_EQ(l.driver.SetChargerMode(ChargerMode::Charge).error, Error::OutOfLimits);
  CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U);
}

// ---- R4-2: the guards that survived ------------------------------------------------------------

TEST_CASE("R4-2 every buffer the port sees, of reads and of writes, is inside the driver object") {
  Live l;
  l.rig.port.state.own_lo = std::bit_cast<std::uintptr_t>(&l.driver);
  l.rig.port.state.own_hi = l.rig.port.state.own_lo + sizeof(l.driver);
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, false, false, false}).ok());
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK(l.driver.ServiceInterrupts().ok());
  CHECK(l.driver.Configure().ok());
  CHECK_EQ(l.rig.port.state.outside, 0);
}

TEST_CASE("R4-2 a start that failed on the DVS declaration still lets a switch-off through") {
  Config c = BaseConfig();
  c.sw1.apply = true; // A2 has SW1 in DVS mode: the declaration below is wrong
  c.sw1.dvs = Dvs::Disabled;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::Refused);
  rig.Chip().Poke(kChgOper, 0x02);
  rig.Chip().writes.Clear();
  CHECK(d.SetChargerMode(ChargerMode::Off).ok());
  CHECK(d.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK(WroteTo(rig, kChgOper));
  CHECK(WroteTo(rig, 0x4D));
}

TEST_CASE("R4-2 a charger found charging above the cell's current limit is out of limits") {
  Config c = BaseConfig();
  c.ack.allow_untimed_charge = true; // nothing else is wrong with the charger as found
  c.limits.cell_max_charge_ma = 500;
  Live l(c);
  l.rig.Chip().Poke(0x8E, 18);          // CHG_CC code 18: 1000 mA
  l.rig.Chip().Poke(kVbusLim, 13 << 3); // 400 mA: the input is inside Limits
  l.rig.Chip().Poke(kChgOper, 0x02);
  CHECK_EQ(l.driver.CheckChargerAgainstLimits().error, Error::OutOfLimits);
}

TEST_CASE("R4-2 a status bit another call read is reported although INTB is high") {
  Live l;
  l.rig.Chip().readings.int_clear = ClearMode::OnRead;
  REQUIRE(l.driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  l.rig.Chip().Raise(0x28, 1);               // PWRDN_I
  REQUIRE(l.driver.ReadRegister(0x28).ok()); // the read clears it in the chip; the driver keeps it
  CHECK(!l.rig.Chip().IntbAsserted());
  const Result<InterruptEvents> e = l.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::PowerDown));
}

TEST_CASE("R4-2 one flipped STATE_INFO read does not open the SHIP gate") {
  Live l;
  l.rig.Chip().SetState(0x00); // the system is not on
  l.rig.port.state.flip_addr = 0x67;
  l.rig.port.state.flip_mask = 0x0C; // the wrong read looks like Run
  l.rig.port.state.flip_count = 1;
  CHECK(l.driver.EnterShip(ShipConfirm{}).error != Error::Ok);
  CHECK(!l.rig.Chip().ship_entered);
}

TEST_CASE("R4-2 one flipped ONKEY read does not open the CORE_OFF gate") {
  Live l;
  l.rig.Chip().env.onkey_pushed = true;
  l.rig.Chip().SetState(0x0C);
  l.rig.port.state.flip_addr = 0x26; // ONKEY_INT_SENSE0
  l.rig.port.state.flip_mask = 0x01; // the wrong read shows the button released
  l.rig.port.state.flip_count = 1;
  CHECK(l.driver.EnterCoreOff(CoreOffConfirm{}).error != Error::Ok);
  CHECK(!l.rig.Chip().core_off_entered);
}

TEST_CASE("R4-2 CancelShutdownRequest writes from two agreeing reads") {
  Live l;
  l.rig.Chip().Poke(0x5B, 0x30);
  l.rig.port.state.flip_addr = 0x5B;
  l.rig.port.state.flip_mask = 0x40; // the first read is wrong
  l.rig.port.state.flip_count = 1;
  CHECK(l.driver.CancelShutdownRequest().ok());
  CHECK_EQ(l.rig.Chip().Peek(0x5B), 0x30);
}

// ---- R4-3: one wrong DEVICE_ID read ------------------------------------------------------------

TEST_CASE("R4-3 one wrong DEVICE_ID read at a re-start does not take the switch-off away") {
  Live l;
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
  l.rig.Chip().Reset(2);
  CHECK_EQ(l.driver.ServiceChargerWatchdog().error, Error::DeviceReset);
  l.rig.port.state.flip_addr = 0x00; // DEVICE_ID
  l.rig.port.state.flip_mask = 0x55;
  l.rig.port.state.flip_count = 1;
  static_cast<void>(l.driver.EnsureInitialized());
  l.rig.Chip().writes.Clear();
  CHECK(l.driver.SetRailModes(Rail::Ldo1, {}).ok());
  CHECK(l.driver.SetChargerMode(ChargerMode::Off).ok());
  CHECK(WroteTo(l.rig, 0x4D));
  CHECK(WroteTo(l.rig, kChgOper));
}
