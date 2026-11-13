/**
 * @file test_lifecycle.cpp
 * @brief Start-up: the one start-up call, identity, the as-found charger check and the guard on
 * every I/O call before it. (Design tests T01, T02, T19, T22.)
 */
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

TEST_CASE("T01 every I/O call returns NotInitialized before EnsureInitialized") {
  Rig rig;
  Driver d(rig.port, rig.cfg);
  CHECK(!d.IsInitialized());
  CHECK_EQ(d.Configure().error, Error::NotInitialized);
  CHECK_EQ(d.ReadVariant().error, Error::NotInitialized);
  CHECK_EQ(d.ReadPowerState().error, Error::NotInitialized);
  CHECK_EQ(d.ReadSnapshot().error, Error::NotInitialized);
  CHECK_EQ(d.SelfCheck().error, Error::NotInitialized);
  CHECK_EQ(d.CheckChargerAgainstLimits().error, Error::NotInitialized);
  CHECK_EQ(d.ReadChargerStatus().error, Error::NotInitialized);
  CHECK_EQ(d.SetChargerMode(ChargerMode::Off).error, Error::NotInitialized);
  CHECK_EQ(d.RestartCharger().error, Error::NotInitialized);
  CHECK_EQ(d.ServiceChargerWatchdog().error, Error::NotInitialized);
  CHECK_EQ(d.ReadRail(Rail::Sw1).error, Error::NotInitialized);
  CHECK_EQ(d.SetRailModes(Rail::Sw1, {}).error, Error::NotInitialized);
  CHECK_EQ(d.SetRailVoltage(Rail::Sw1, FromMillivolts(1000)).error, Error::NotInitialized);
  CHECK_EQ(d.EnableLoadSwitch(Rail::Ldo1, {}).error, Error::NotInitialized);
  CHECK_EQ(d.RequestStandby(true).error, Error::NotInitialized);
  CHECK_EQ(d.EnterShip({}).error, Error::NotInitialized);
  CHECK_EQ(d.EnterCoreOff({}).error, Error::NotInitialized);
  CHECK_EQ(d.ReadPowerDownSequence().error, Error::NotInitialized);
  CHECK_EQ(d.SetPowerDownSequence({}).error, Error::NotInitialized);
  CHECK_EQ(d.SetIrqMask({}).error, Error::NotInitialized);
  CHECK_EQ(d.ReadCategories().error, Error::NotInitialized);
  CHECK_EQ(d.ReadSense(IrqGroup::Misc).error, Error::NotInitialized);
  CHECK_EQ(d.ServiceInterrupts().error, Error::NotInitialized);
  CHECK_EQ(d.IntbAsserted().error, Error::NotInitialized);
  CHECK_EQ(rig.port.state.transfers, 0);
}

TEST_CASE("T02 EnsureInitialized reads DEVICE_ID and the charger, writes nothing") {
  Rig rig;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK(d.IsInitialized());
  CHECK_EQ(WriteCount(rig), 0U); // PWRUP_I is no reset flag (Table 108), so it is not touched
  CHECK_EQ(rig.port.state.bus_up_calls, 1);
  const int transfers = rig.port.state.transfers;
  CHECK(d.EnsureInitialized().ok()); // idempotent: no further traffic
  CHECK_EQ(rig.port.state.transfers, transfers);
  CHECK(rig.port.log_lines > 0);
}

TEST_CASE("T02 Config without Limits is rejected before any I/O") {
  Rig rig(2, Config{});
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(rig.port.state.transfers, 0);
  CHECK_EQ(rig.port.state.bus_up_calls, 0);
}

TEST_CASE("T02 failures of the bus and of identity") {
  {
    Rig rig;
    rig.port.state.bus_up_ok = false;
    Driver d(rig.port, rig.cfg);
    CHECK_EQ(d.EnsureInitialized().error, Error::Bus);
  }
  {
    Rig rig;
    rig.port.state.address = 0x0F; // the chip answers elsewhere: the address is not acknowledged
    Driver d(rig.port, rig.cfg);
    CHECK_EQ(d.EnsureInitialized().error, Error::NoDevice);
    CHECK(!d.IsInitialized());
  }
  {
    Rig rig;
    rig.port.state.fail_transfer = 0; // a data NACK is a plain bus error
    Driver d(rig.port, rig.cfg);
    CHECK_EQ(d.EnsureInitialized().error, Error::Bus);
  }
  {
    Rig rig;
    rig.Chip().Poke(0x00, 0x7D);
    Driver d(rig.port, rig.cfg);
    CHECK_EQ(d.EnsureInitialized().error, Error::DeviceIdMismatch);
    rig.Chip().Poke(0x00, 0x7C);
    CHECK(d.EnsureInitialized().ok()); // can be repeated
  }
  {
    MinimalPort port; // without GetLastFailure() every failure is Bus
    port.state.address = 0x0F;
    PF1550<MinimalPort> d(port, BaseConfig());
    CHECK_EQ(d.EnsureInitialized().error, Error::Bus);
  }
}

TEST_CASE("T19 as found: option A1 starts the charger with no fast-charge timer") {
  Rig rig(1);
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error,
           Error::Unsafe); // Report: return the error, no charger write
  CHECK(!WroteTo(rig, 0x89));
  CHECK(!d.IsInitialized());
}

TEST_CASE("T19 as found: MakeSafe writes CHG_OPER = linear only and verifies") {
  Config c = BaseConfig();
  c.as_found_policy = AsFoundPolicy::MakeSafe;
  Rig rig(1, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(WriteCount(rig), 1U);
  CHECK(LastWriteWas(rig, 0x89, 0x01));
  const Result<ChargerAsFound> found = d.CheckChargerAgainstLimits();
  CHECK(found.ok());
  CHECK(found.value.safe);
  CHECK(found.value.mode == ChargerMode::LinearOnly);
}

TEST_CASE("T19 as found: acknowledgements make an untimed charger acceptable") {
  Config c = BaseConfig();
  c.ack.allow_untimed_charge = true;
  Rig rig(1, c);
  Driver d(rig.port, rig.cfg);
  CHECK(d.EnsureInitialized().ok());
  CHECK(!WroteTo(rig, 0x89));
}

TEST_CASE("T19 as found: limits below the OTP charge values give OutOfLimits") {
  Config c = BaseConfig();
  c.ack.allow_untimed_charge = true;
  c.limits.cell_max_mv = 4100; // the option charges to 4.2 V
  Rig rig(3, c);
  Driver d(rig.port, rig.cfg);
  CHECK_EQ(d.EnsureInitialized().error, Error::OutOfLimits);
  Config lower = BaseConfig();
  lower.ack.allow_untimed_charge = true;
  lower.limits.cell_max_charge_ma = 400; // option A3 charges at 500 mA
  Rig rig2(3, lower);
  Driver d2(rig2.port, rig2.cfg);
  CHECK_EQ(d2.EnsureInitialized().error, Error::OutOfLimits);
  Config input = BaseConfig();
  input.ack.allow_untimed_charge = true;
  input.limits.input_max_ma = 500; // 1.5 A input limit
  Rig rig3(3, input);
  Driver d3(rig3.port, rig3.cfg);
  CHECK_EQ(d3.EnsureInitialized().error, Error::OutOfLimits);
}

TEST_CASE("T19 as found: a charger that is off is not checked") {
  Config c = BaseConfig();
  c.limits.cell_max_mv = 3500; // far below the OTP 4.2 V, but charging is off
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  CHECK(d.EnsureInitialized().ok());
}

TEST_CASE("T19 as found: each OTP option reads back its identity") {
  for (uint8_t option = 1; option <= 3; ++option) {
    Config c = BaseConfig();
    c.as_found_policy = AsFoundPolicy::MakeSafe;
    Rig rig(option, c);
    Driver d(rig.port, rig.cfg);
    REQUIRE(d.EnsureInitialized().ok());
    const Result<VariantInfo> v = d.ReadVariant();
    REQUIRE(v.ok());
    CHECK_EQ(v.value.device_id, 0x7C);
    CHECK_EQ(v.value.flavor_raw, option);
    CHECK_EQ(static_cast<int>(v.value.option), static_cast<int>(option));
    CHECK_EQ(v.value.i2c_slave_addr_code, 0);
  }
}

TEST_CASE("T22 variants: A0 and unlisted flavour bytes are reported, never an error") {
  Rig rig(0);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  Result<VariantInfo> v = d.ReadVariant();
  CHECK(v.ok());
  CHECK(v.value.option == Option::A0);
  rig.Chip().Poke(0x01, 0x2A);
  v = d.ReadVariant();
  CHECK(v.ok());
  CHECK(v.value.option == Option::Unknown);
  CHECK_EQ(v.value.flavor_raw, 0x2A);
}

TEST_CASE("T22 SILICON_REV and the address code are reported raw") {
  Rig rig;
  rig.Chip().readings.silicon_rev_reset = 0x08;
  rig.Chip().Reset();
  rig.Chip().Poke(0x68, 0x05);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  const Result<VariantInfo> v = d.ReadVariant();
  CHECK_EQ(v.value.silicon_rev_raw, 0x08);
  CHECK_EQ(v.value.i2c_slave_addr_code, 5);
}

TEST_CASE("T22 power states decode and a reserved code is Unknown") {
  Rig rig;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  const struct {
    uint8_t code;
    PowerState state;
  } cases[] = {{0x00, PowerState::Wait},        {0x0C, PowerState::Run},
               {0x0D, PowerState::Standby},     {0x0E, PowerState::Sleep},
               {0x2B, PowerState::RegsDisable}, {0x01, PowerState::Unknown}};
  for (const auto& c : cases) {
    rig.Chip().SetState(c.code);
    const Result<PowerState> s = d.ReadPowerState();
    CHECK(s.ok());
    CHECK(s.value == c.state);
  }
}
