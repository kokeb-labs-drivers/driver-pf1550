/**
 * @file test_rails.cpp
 * @brief Rail readout, voltage and enable changes, the load switch. (T20 to T23.)
 */
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
Config RailConfig() {
  Config c = BaseConfig();
  c.sw1.dvs = Dvs::Enabled;
  c.sw2.dvs = Dvs::Disabled;
  c.ack.dvs_on_unlisted_part = true; // the rigs use options 1 to 4
  return c;
}
} // namespace

TEST_CASE("T20 ReadRail decodes each option's start-up values") {
  Rig rig(4, RailConfig());
  rig.cfg.as_found_policy = AsFoundPolicy::MakeSafe;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  Result<RailReadout> r = d.ReadRail(Rail::Sw1);
  REQUIRE(r.ok());
  CHECK_EQ(r.value.volt.value, 1100000U); // A4: 1.1 V with DVS (code 40)
  CHECK(r.value.enabled_run && r.value.enabled_standby && !r.value.enabled_sleep);
  r = d.ReadRail(Rail::Sw2);
  CHECK_EQ(r.value.volt.value,
           3300000U); // declared non-DVS: code 48 reads as 3.3 V in the fixed table
  r = d.ReadRail(Rail::Sw3);
  CHECK_EQ(r.value.volt.value, 1800000U);
  r = d.ReadRail(Rail::Ldo1);
  CHECK_EQ(r.value.volt.value, 3300000U);
  r = d.ReadRail(Rail::Ldo2);
  CHECK_EQ(r.value.volt.value, 3300000U);
  r = d.ReadRail(Rail::Ldo3);
  CHECK_EQ(r.value.volt.value, 1800000U);
  r = d.ReadRail(Rail::VrefDdr);
  CHECK_EQ(r.value.volt.value, 0U);
  CHECK(r.value.enabled_run);
  CHECK_EQ(d.ReadRail(static_cast<Rail>(7)).error, Error::InvalidArgument);
}

TEST_CASE("T20 load switch and low power flags read back") {
  Rig rig(1, RailConfig());
  rig.cfg.as_found_policy = AsFoundPolicy::MakeSafe;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK(d.ReadRail(Rail::Ldo3).value.load_switch); // option A1 starts LDO3 as a load switch
  CHECK(!d.ReadRail(Rail::Ldo1).value.load_switch);
  rig.Chip().Poke(0x4D, 0x0F);
  const Result<RailReadout> r = d.ReadRail(Rail::Ldo1);
  CHECK(r.value.low_power && r.value.enabled_sleep);
  CHECK(!d.ReadRail(Rail::Sw1).value.load_switch);
}

TEST_CASE("T21 SetRailVoltage: LDOs, DVS bucks and the refusals") {
  Rig rig(2, RailConfig());
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK(d.SetRailVoltage(Rail::Ldo1, FromMillivolts(2800)).ok());
  CHECK_EQ(rig.Chip().Peek(0x4C), 26);
  CHECK(d.SetRailVoltage(Rail::Ldo2, FromMillivolts(2500)).ok());
  CHECK_EQ(rig.Chip().Peek(0x4F), 7);
  CHECK(d.SetRailVoltage(Rail::Ldo3, FromMillivolts(1800)).ok());
  CHECK(d.SetRailVoltage(Rail::Sw1, FromMillivolts(1000)).ok()); // A2: SW1 at 1.0 V, off
  CHECK_EQ(d.SetRailVoltage(Rail::Sw1, FromMillivolts(1300)).error, Error::OutOfLimits);
  CHECK_EQ(d.SetRailVoltage(Rail::Sw1, Microvolts{1001000}).error, Error::InvalidArgument);
  CHECK_EQ(d.SetRailVoltage(Rail::Sw1, Microvolts{0}).error, Error::InvalidArgument);
  CHECK_EQ(d.SetRailVoltage(Rail::Sw2, FromMillivolts(1200)).error, Error::ReadOnlyRail); // not DVS
  CHECK_EQ(d.SetRailVoltage(Rail::Sw3, FromMillivolts(1800)).error, Error::ReadOnlyRail);
  CHECK_EQ(d.SetRailVoltage(Rail::VrefDdr, FromMillivolts(750)).error, Error::InvalidArgument);
  CHECK_EQ(d.SetRailVoltage(Rail::Ldo1, FromMillivolts(1650)).error, Error::InvalidArgument);
  CHECK_EQ(d.SetRailVoltage(Rail::Ldo1, FromMillivolts(1000)).error, Error::OutOfLimits);
  CHECK_EQ(d.SetRailVoltage(static_cast<Rail>(9), FromMillivolts(1000)).error,
           Error::InvalidArgument);
  CHECK(!WroteTo(rig, 0x3E) && !WroteTo(rig, 0x3F) && !WroteTo(rig, 0x40));
  CHECK_EQ(rig.Chip().protected_writes, 0U);
}

TEST_CASE("T21 a rail with no window is never driven") {
  Config c = RailConfig();
  c.limits.rail[static_cast<std::size_t>(Rail::Ldo2)] = {};
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.SetRailVoltage(Rail::Ldo2, FromMillivolts(3300)).error, Error::OutOfLimits);
  CHECK_EQ(d.SetRailModes(Rail::Ldo2, {true, false, false, false}).error, Error::OutOfLimits);
  CHECK(d.SetRailModes(Rail::Ldo2, {false, false, false, false})
            .ok()); // switching off is always allowed
}

TEST_CASE("T22 SetRailModes writes the four enables and checks the low power rule") {
  Rig rig(2, RailConfig());
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK(d.SetRailModes(Rail::Ldo3, {true, false, true, false}).ok());
  CHECK_EQ(rig.Chip().Peek(0x53) & 0x0FU, 0x05U);
  CHECK_EQ(d.SetRailModes(Rail::Ldo3, {true, true, true, true}).error, Error::Unsafe);
  CHECK_EQ(rig.Chip().Peek(0x53) & 0x0FU, 0x05U);
  CHECK_EQ(d.SetRailModes(static_cast<Rail>(8), {}).error, Error::InvalidArgument);
  Config c = RailConfig();
  c.ack.load_below_10ma = true;
  Rig rig2(2, c);
  Driver d2(rig2.port, rig2.cfg);
  REQUIRE(d2.EnsureInitialized().ok());
  CHECK(d2.SetRailModes(Rail::Sw1, {true, true, true, true}).ok());
  CHECK_EQ(rig2.Chip().Peek(0x35) & 0x0FU, 0x0FU);
}

TEST_CASE("T23 the load switch is one way and needs a window and a token") {
  Rig rig(2, RailConfig());
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.EnableLoadSwitch(Rail::Ldo2, LoadSwitchConfirm{}).error, Error::InvalidArgument);
  CHECK_EQ(d.EnableLoadSwitch(Rail::Sw1, LoadSwitchConfirm{}).error, Error::InvalidArgument);
  CHECK(d.EnableLoadSwitch(Rail::Ldo1, LoadSwitchConfirm{.input_mv = 1800}).ok());
  CHECK((rig.Chip().Peek(0x4D) & 0x10U) != 0U);
  CHECK(d.ReadRail(Rail::Ldo1).value.load_switch);
  Config c = RailConfig();
  c.limits.rail[static_cast<std::size_t>(Rail::Ldo3)] = {};
  Rig rig2(2, c);
  Driver d2(rig2.port, rig2.cfg);
  REQUIRE(d2.EnsureInitialized().ok());
  CHECK_EQ(d2.EnableLoadSwitch(Rail::Ldo3, LoadSwitchConfirm{.input_mv = 1800}).error,
           Error::OutOfLimits);
  CHECK((rig2.Chip().Peek(0x53) & 0x10U) == 0U);
  // No API clears the bit again: the control register writes of the driver never carry LS_EN = 0.
  rig.Chip().writes.Clear();
  CHECK(d.SetRailModes(Rail::Ldo1, {true, true, false, false}).ok());
  CHECK((rig.Chip().Peek(0x4D) & 0x10U) != 0U);
}
