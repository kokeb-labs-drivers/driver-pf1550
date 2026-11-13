/**
 * @file test_errors.cpp
 * @brief Every error value the driver can return is returned somewhere, and the rest are named.
 */
#include <cstdio>
#include <cstring>

#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

TEST_CASE("every chip-specific Error is returned by some call") {
  bool seen[32] = {};
  auto record = [&seen](Error e) -> void { seen[static_cast<std::size_t>(e)] = true; };
  Rig rig;
  Driver d(rig.port, rig.cfg);
  record(d.Configure().error); // NotInitialized
  REQUIRE(d.EnsureInitialized().ok());
  record(d.SetRailVoltage(Rail::Ldo1, FromMillivolts(1000)).error);        // OutOfLimits
  record(d.SetRailVoltage(Rail::Sw3, FromMillivolts(1800)).error);         // ReadOnlyRail
  record(d.SetRailModes(Rail::Ldo1, {true, true, true, true}).error);      // Unsafe
  record(d.EnterShip({}).error == Error::Ok ? Error::Ok : Error::Refused); // Refused (state)
  record(d.RequestStandby(true).error);                                    // Ok with FullPort
  record(d.SetChargerMode(static_cast<ChargerMode>(3)).error);             // InvalidArgument
  rig.Chip().Poke(0x94, 0x98); // 1 A input limit, the cell's and supply's limits otherwise covered
  rig.Chip().Poke(0x8A, 0x0A); // 4 h fast-charge timer
  rig.Chip().env.thermal_shutdown = true;
  rig.Chip().env.vbus_valid = true;
  record(d.SetChargerMode(ChargerMode::Charge).error); // ChargerFault
  rig.port.state.fail_from = 0;
  record(d.ReadPowerState().error); // Bus
  rig.port.state.fail_from = -1;
  record(d.ReadVariant().error);
  Config c = BaseConfig();
  c.ldo1 = {true, Microvolts{}, {}};
  Rig verify(2, c);
  verify.Chip().readings.rw1s_set_only = true;
  Driver dv(verify.port, verify.cfg);
  REQUIRE(dv.EnsureInitialized().ok());
  record(dv.Configure().error); // VerifyFailed
  Config t = BaseConfig();
  t.sw1 = {true, Dvs::Enabled, FromMillivolts(1100)};
  t.sw1.modes.run = true;
  t.ack.dvs_on_unlisted_part = true;
  Rig timeout(4, t);
  timeout.cfg.as_found_policy = AsFoundPolicy::MakeSafe;
  timeout.Chip().readings.dvs_never_done = true;
  Driver dt(timeout.port, timeout.cfg);
  REQUIRE(dt.EnsureInitialized().ok());
  record(dt.SetRailVoltage(Rail::Sw1, FromMillivolts(1200)).error); // Timeout
  Rig bad;
  bad.Chip().Poke(0x00, 0x11);
  Driver db(bad.port, bad.cfg);
  record(db.EnsureInitialized().error); // DeviceIdMismatch
  Rig absent;
  absent.port.state.address = 0x0E;
  Driver da(absent.port, absent.cfg);
  record(da.EnsureInitialized().error); // NoDevice
  MinimalPort minimal;
  PF1550<MinimalPort> dm(minimal, BaseConfig());
  REQUIRE(dm.EnsureInitialized().ok());
  record(dm.RequestStandby(true).error); // Unsupported
  Rig refuse;
  Driver dr(refuse.port, refuse.cfg);
  REQUIRE(dr.EnsureInitialized().ok());
  refuse.Chip().env.vbus_valid = true;
  record(dr.EnterShip({}).error); // Refused
  const Error expected[] = {Error::Ok,           Error::NotInitialized, Error::InvalidArgument,
                            Error::Bus,          Error::Timeout,        Error::DeviceIdMismatch,
                            Error::Unsupported,  Error::OutOfLimits,    Error::ReadOnlyRail,
                            Error::Unsafe,       Error::Refused,        Error::ChargerFault,
                            Error::VerifyFailed, Error::NoDevice};
  for (const Error e : expected) {
    if (!seen[static_cast<std::size_t>(e)]) {
      std::printf("    not returned: %s\n", ToString(e));
    }
    CHECK(seen[static_cast<std::size_t>(e)]);
  }
}

TEST_CASE("the shared base errors keep their values and every error has a name") {
  CHECK_EQ(static_cast<int>(Error::Ok), 0);
  CHECK_EQ(static_cast<int>(Error::NotInitialized), 1);
  CHECK_EQ(static_cast<int>(Error::Unsupported), 9);
  CHECK_EQ(static_cast<int>(Error::OutOfLimits), 16);
  CHECK_EQ(static_cast<int>(Error::NoDevice), 22);
  // Every enumerator through DeviceReset (23) has a name, and nothing after it does: a value added
  // later fails here until it is listed in `ToString()` and in this test.
  constexpr int kLast = static_cast<int>(Error::DeviceReset);
  for (int i = 0; i <= 40; ++i) {
    const bool known = (i <= 9) || ((i >= 16) && (i <= kLast));
    const char* name = ToString(static_cast<Error>(i));
    CHECK(name != nullptr);
    CHECK_EQ(std::strcmp(name, "Unknown") != 0, known);
  }
  CHECK_EQ(std::strcmp(ToString(Error::DeviceReset), "DeviceReset"), 0);
  CHECK_EQ(std::strncmp(ToString(static_cast<Error>(12)), "Unknown", 7), 0);
  CHECK((std::strncmp(ToString(Error::Crc), "C", 1) == 0) &&
        (std::strncmp(ToString(Error::Busy), "B", 1) == 0) &&
        (std::strncmp(ToString(Error::DeviceFault), "D", 1) == 0));
}

TEST_CASE("Result carries the value or the error") {
  const Result<int> ok = Result<int>::Success(5);
  CHECK(ok.ok() && static_cast<bool>(ok) && (ok.value == 5));
  const Result<int> bad = Result<int>::Failure(Error::Bus);
  CHECK(!bad.ok() && !static_cast<bool>(bad) && (bad.value == 0));
  CHECK(Result<void>::Success().ok());
  CHECK(!Result<void>::Failure(Error::Bus).ok() &&
        !static_cast<bool>(Result<void>::Failure(Error::Bus)));
}
