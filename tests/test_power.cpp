/**
 * @file test_power.cpp
 * @brief Power states: SHIP and CORE_OFF, the power-down order, the STANDBY pin, the sequencer
 * model and the coin cell charger. (T24 to T30.)
 */
#include "pf1550_sequencer.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
struct Ready {
  Rig rig;
  Driver driver;
  explicit Ready(const Config& c = BaseConfig()) : rig(2, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
};
} // namespace

TEST_CASE("T24 EnterShip is refused with VBUS present and in a state where the host cannot write") {
  Ready r;
  r.rig.Chip().env.vbus_valid = true;
  CHECK_EQ(r.driver.EnterShip(ShipConfirm{}).error, Error::Refused);
  CHECK(!WroteTo(r.rig, 0x5B));
  r.rig.Chip().env.vbus_valid = false;
  r.rig.Chip().SetState(0x2B); // REGS_DISABLE
  CHECK_EQ(r.driver.EnterShip(ShipConfirm{}).error, Error::Refused);
  r.rig.Chip().SetState(0x00); // WAIT
  CHECK_EQ(r.driver.EnterShip(ShipConfirm{}).error, Error::Refused);
  r.rig.Chip().SetState(0x0C);
  r.rig.Chip().Poke(0x86, 0x00); // VBUS neither valid nor below UVLO: between 1 V and UVLO
  CHECK_EQ(r.driver.EnterShip(ShipConfirm{}).error, Error::Refused);
  CHECK(!WroteTo(r.rig, 0x5B));
}

TEST_CASE("T24 EnterShip writes GOTO_SHIP when VBUS is gone, from every system-on state") {
  for (const uint8_t state : {uint8_t{0x0C}, uint8_t{0x0D}, uint8_t{0x0E}}) {
    Ready r;
    r.rig.Chip().SetState(state);
    REQUIRE(r.driver.EnterShip(ShipConfirm{}).ok());
    CHECK(LastWriteWas(r.rig, 0x5B, 0x01));
    CHECK(r.rig.Chip().ship_entered);
  }
}

TEST_CASE("T25 EnterCoreOff needs VBUS invalid and ONKEY high") {
  Ready r;
  r.rig.Chip().env.onkey_pushed = true;
  CHECK_EQ(r.driver.EnterCoreOff(CoreOffConfirm{}).error, Error::Refused);
  r.rig.Chip().env.onkey_pushed = false;
  r.rig.Chip().env.vbus_valid = true;
  CHECK_EQ(r.driver.EnterCoreOff(CoreOffConfirm{}).error, Error::Refused);
  r.rig.Chip().env.vbus_valid = false;
  r.rig.Chip().SetState(0x00);
  CHECK_EQ(r.driver.EnterCoreOff(CoreOffConfirm{}).error, Error::Refused);
  CHECK(!WroteTo(r.rig, 0x5B));
  r.rig.Chip().SetState(0x0C);
  REQUIRE(r.driver.EnterCoreOff(CoreOffConfirm{}).ok());
  CHECK(LastWriteWas(r.rig, 0x5B, 0x02));
  CHECK(r.rig.Chip().core_off_entered);
  CHECK(!r.rig.Chip().ship_entered);
}

TEST_CASE("T25 a corrupted read of PWRCTRL3 never turns one command into the other") {
  Ready r;
  r.rig.Chip().SetState(0x0C);
  r.rig.Chip().Poke(0x5B, 0x01); // the read returns GOTO_SHIP set, as a noisy bus can
  REQUIRE(r.driver.EnterCoreOff(CoreOffConfirm{}).ok());
  CHECK(LastWriteWas(r.rig, 0x5B, 0x02));
  CHECK(!r.rig.Chip().ship_entered);
  Ready s;
  s.rig.Chip().SetState(0x0C);
  s.rig.Chip().Poke(0x5B, 0x02); // and GOTO_CORE_OFF set
  REQUIRE(s.driver.EnterShip(ShipConfirm{}).ok());
  CHECK(LastWriteWas(s.rig, 0x5B, 0x01));
  CHECK(!s.rig.Chip().core_off_entered);
}

TEST_CASE("T25 a bus error while reading the preconditions writes nothing") {
  Ready r;
  r.rig.port.state.fail_from = r.rig.port.state.transfers + 2;
  CHECK_EQ(r.driver.EnterShip(ShipConfirm{}).error, Error::Bus);
  CHECK_EQ(r.driver.EnterCoreOff(CoreOffConfirm{}).error, Error::Bus);
  CHECK(!WroteTo(r.rig, 0x5B));
}

TEST_CASE("T26 the power-down order: read, write with the acknowledgement, refuse without") {
  Config c = BaseConfig();
  c.ack.power_down_order_reviewed = true;
  Ready r(c);
  Result<PowerDownSequence> s = r.driver.ReadPowerDownSequence();
  REQUIRE(s.ok());
  CHECK_EQ(s.value.order[0], 5); // option A2 mirrors its power-up order
  CHECK_EQ(s.value.order[2], 1);
  PowerDownSequence next{};
  next.order = {1, 2, 3, 4, 5, 6, 7};
  REQUIRE(r.driver.SetPowerDownSequence(next).ok());
  s = r.driver.ReadPowerDownSequence();
  CHECK(s.value.order == next.order);
  next.order[3] = 8;
  CHECK_EQ(r.driver.SetPowerDownSequence(next).error, Error::InvalidArgument);
  Ready plain;
  CHECK_EQ(plain.driver.SetPowerDownSequence(PowerDownSequence{}).error, Error::Unsafe);
  CHECK(!WroteTo(plain.rig, 0x5F));
}

TEST_CASE("T26 Configure writes the power-down order from the configuration") {
  Config c = BaseConfig();
  c.ack.power_down_order_reviewed = true;
  c.power_down.apply = true;
  c.power_down.sequence.order = {0, 0, 0, 0, 0, 0, 0};
  Ready r(c);
  REQUIRE(r.driver.Configure().ok());
  for (uint8_t a = 0x5F; a <= 0x65; ++a) {
    CHECK_EQ(r.rig.Chip().Peek(a) & 7U, 0U);
  }
  CHECK(IsPowerDownBypassed(r.driver.ReadPowerDownSequence().value));
}

TEST_CASE("T27 the STANDBY pin goes through the port, or the call is Unsupported") {
  Ready r;
  CHECK(r.driver.RequestStandby(true).ok());
  CHECK(r.rig.port.standby_pin);
  CHECK(r.driver.RequestStandby(false).ok());
  CHECK(!r.rig.port.standby_pin);
  r.rig.port.gpio_ok = false;
  CHECK_EQ(r.driver.RequestStandby(true).error, Error::Bus);
  MinimalPort port;
  PF1550<MinimalPort> d(port, BaseConfig());
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.RequestStandby(true).error, Error::Unsupported);
  CHECK_EQ(d.IntbAsserted().error, Error::Unsupported);
}

TEST_CASE("T28 the start-up sequence of every option matches the model") {
  for (uint8_t o = 1; o <= 9; ++o) {
    Pf1550Model chip(o);
    const auto profile = StartupProfileOf(static_cast<Option>(o));
    REQUIRE(profile.ok());
    const auto events = chip.PowerUp();
    uint32_t last = 0;
    for (std::size_t i = 0; i < events.size(); ++i) {
      CHECK_EQ(events[i].time_us, PowerUpDelayUs(profile.value, static_cast<Rail>(i)));
      last = events[i].time_us > last ? events[i].time_us : last;
    }
    CHECK_EQ(last, PowerUpDurationUs(profile.value));
    CHECK_EQ(chip.ResetReleaseUs(), ResetReleaseDelayMs(0).value * 1000U);
    CHECK_EQ(ColdStartToResetReleaseUs(profile.value, 0).value,
             kVsnvsTurnOnDelayUs + last + chip.ResetReleaseUs());
    CHECK_EQ(profile.value.pwron_edge, o == 8);
    CHECK_EQ(profile.value.slot_us, o == 1 ? 500U : 2000U);
  }
  CHECK(!StartupProfileOf(Option::A0).ok());
}

TEST_CASE("T28 POR_DLY codes give the RESETBMCU delay of Table 135") {
  Pf1550Model chip(2);
  const struct {
    uint8_t code;
    uint32_t ms;
  } cases[] = {{0, 2}, {2, 4}, {3, 8}, {4, 16}, {5, 128}, {6, 256}, {7, 1024}};
  for (const auto& c : cases) {
    chip.Poke(0x58, static_cast<uint8_t>(c.code << 3));
    CHECK_EQ(chip.ResetReleaseUs(), c.ms * 1000U);
    CHECK_EQ(ResetReleaseDelayMs(c.code).value, c.ms);
  }
  CHECK(!ResetReleaseDelayMs(1).ok());
  CHECK(!ResetReleaseDelayMs(8).ok());
  CHECK(!ColdStartToResetReleaseUs(StartupProfileOf(Option::A1).value, 1).ok());
}

TEST_CASE("T29 the power-down sequence mirrors the power-up order and follows the registers") {
  for (uint8_t o = 1; o <= 9; ++o) {
    Pf1550Model chip(o);
    const auto profile = StartupProfileOf(static_cast<Option>(o)).value;
    const PowerDownSequence mirror = MirrorOf(profile);
    const auto events = chip.PowerDown();
    for (std::size_t i = 0; i < events.size(); ++i) {
      CHECK_EQ(events[i].time_us, PowerDownDelayUs(mirror, static_cast<Rail>(i)));
    }
    CHECK_EQ(PowerDownDurationUs(mirror) > 0U, true);
  }
  Config c = BaseConfig();
  c.ack.power_down_order_reviewed = true;
  Ready w(c);
  PowerDownSequence s{};
  s.order = {7, 0, 3, 3, 1, 0, 2};
  REQUIRE(w.driver.SetPowerDownSequence(s).ok());
  const auto events = w.rig.Chip().PowerDown();
  for (std::size_t i = 0; i < events.size(); ++i) {
    CHECK_EQ(events[i].time_us, PowerDownDelayUs(s, static_cast<Rail>(i)));
  }
  CHECK_EQ(PowerDownDelayUs(s, Rail::Ldo1),
           PowerDownDelayUs(s, Rail::Sw3));                         // equal order: together (C55)
  CHECK_EQ(PowerDownDelayUs(s, Rail::Sw2), PowerDownDurationUs(s)); // order 0 turns off last (C56)
  CHECK_EQ(PowerDownDelayUs(s, Rail::Sw1), 0U);
  CHECK_EQ(PowerDownDurationUs(s), 14000U);
  CHECK(!IsPowerDownBypassed(s));
}

TEST_CASE("T30 the coin cell charger: voltage and enable reach the register, the model charges") {
  Config c = BaseConfig();
  c.ack.coin_cell_is_rechargeable = true;
  c.coin = {true, true, 2500};
  Ready r(c);
  CHECK_EQ(r.rig.Chip().CoinChargeMv(), 0U);
  REQUIRE(r.driver.Configure().ok());
  CHECK_EQ(r.rig.Chip().CoinChargeMv(), 2500U);
  Config off = BaseConfig();
  off.coin = {true, false, 1800};
  Ready o(off);
  REQUIRE(o.driver.Configure().ok());
  CHECK_EQ(o.rig.Chip().CoinChargeMv(), 0U);
}
