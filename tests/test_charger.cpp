/**
 * @file test_charger.cpp
 * @brief Charger status, fault classification, mode changes, restart and the watchdog. (T15 to
 * T19.)
 */
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {

struct Ready {
  Rig rig;
  Driver driver;
  explicit Ready(const Config& c = ChargerConfigOn()) : rig(2, c), driver(rig.port, rig.cfg) {
    rig.Chip().env.vbus_valid = true;
    CHECK(driver.EnsureInitialized().ok());
  }
};

} // namespace

TEST_CASE("T15 charger states decode from CHG_SNS, including reserved codes") {
  Ready r;
  const struct {
    uint8_t code;
    ChargerState state;
    Fault fault;
  } cases[] = {{0, ChargerState::Precharge, Fault::None},
               {1, ChargerState::FastChargeCc, Fault::None},
               {2, ChargerState::FastChargeCv, Fault::None},
               {3, ChargerState::EndOfCharge, Fault::None},
               {4, ChargerState::Done, Fault::None},
               {5, ChargerState::Unknown, Fault::UnknownState},
               {6, ChargerState::TimerFault, Fault::TimerFault},
               {7, ChargerState::ThermistorSuspend, Fault::ThermistorSuspend},
               {8, ChargerState::Off, Fault::None},
               {9, ChargerState::BatteryOvervoltage, Fault::BatteryOvervoltage},
               {10, ChargerState::ThermalShutdown, Fault::ThermalShutdown},
               {12, ChargerState::LinearOnly, Fault::None},
               {13, ChargerState::Unknown, Fault::UnknownState}};
  for (const auto& c : cases) {
    r.rig.Chip().Poke(0x87, c.code);
    r.rig.Chip().Poke(0x88, 4);
    const Result<ChargerStatus> s = r.driver.ReadChargerStatus();
    // The model recomputes CHG_SNS from the environment on every refresh; Poke is the last word.
    CHECK(s.ok());
    CHECK(s.value.state == c.state);
    CHECK(s.value.fault == c.fault);
    CHECK(s.value.consistent);
  }
}

TEST_CASE("T15 battery states, flags and the watchdog readings") {
  Ready r;
  r.rig.Chip().Poke(0x87, 0x08U | 0x20U | 0x40U | 0x80U);
  r.rig.Chip().Poke(0x88, 0x24);
  Result<ChargerStatus> s = r.driver.ReadChargerStatus();
  CHECK(s.value.watchdog_expired && s.value.thermistor_outside && s.value.thermal_regulation);
  CHECK(s.value.battery_overcurrent);
  CHECK(s.value.battery == BatteryState::AbovePrecharge);
  CHECK(s.value.fault == Fault::BatteryOvercurrent);
  r.rig.Chip().Poke(0x87, 0x0B); // the watchdog expiry code of section 8.9.2 (C9)
  r.rig.Chip().Poke(0x88, 0x06);
  s = r.driver.ReadChargerStatus();
  CHECK(s.value.watchdog_expired);
  CHECK(s.value.battery == BatteryState::NotDetected);
  CHECK(s.value.fault == Fault::WatchdogExpired);
  r.rig.Chip().Poke(0x87, 0x08);
  r.rig.Chip().Poke(0x88, 0x06);
  CHECK(r.driver.ReadChargerStatus().value.fault == Fault::BatteryMissing);
  r.rig.Chip().Poke(0x88, 0x03);
  CHECK(r.driver.ReadChargerStatus().value.fault == Fault::UnknownState);
  r.rig.Chip().Poke(0x88, 0x05);
  CHECK(r.driver.ReadChargerStatus().value.fault == Fault::BatteryOvervoltage);
  r.rig.Chip().Poke(0x88, 0x01);
  CHECK(r.driver.ReadChargerStatus().value.battery == BatteryState::BelowPrecharge);
  r.rig.Chip().Poke(0x88, 0x02);
  CHECK(r.driver.ReadChargerStatus().value.battery == BatteryState::CannotChargePastPrecharge);
}

TEST_CASE("T15 VBUS flags decode") {
  Ready r;
  r.rig.Chip().env.vbus_ovlo = true;
  r.rig.Chip().env.vbus_dpm = true;
  const Result<ChargerStatus> s = r.driver.ReadChargerStatus();
  CHECK(s.value.vbus.valid && s.value.vbus.ovlo && s.value.vbus.dpm);
  CHECK(!s.value.vbus.uvlo);
  r.rig.Chip().env.vbus_valid = false;
  r.rig.Chip().Poke(0x00, 0x7C);
  CHECK(r.driver.ReadChargerStatus().value.vbus.uvlo);
}

TEST_CASE("T16 a charger state that changes between the two reads is flagged") {
  Ready r;
  // The mock cannot change between two reads by itself; check the flag through the decoder.
  const ChargerStatus s = detail::BuildChargerStatus(0x20, 0x01, 0x04, 0x02);
  CHECK(!s.consistent);
  CHECK(detail::BuildChargerStatus(0x20, 0x01, 0x04, 0x01).consistent);
}

TEST_CASE("T17 charging is enabled only with limits-covered registers and no fault") {
  Ready r;
  REQUIRE(r.driver.Configure().ok());
  CHECK(r.driver.SetChargerMode(ChargerMode::Charge).ok());
  CHECK_EQ(r.rig.Chip().Peek(0x89) & 3U, 2U);
  CHECK(r.driver.SetChargerMode(ChargerMode::LinearOnly).ok());
  CHECK_EQ(r.rig.Chip().Peek(0x89) & 3U, 1U);
  CHECK(r.driver.SetChargerMode(ChargerMode::Off).ok());
  CHECK_EQ(r.rig.Chip().Peek(0x89) & 3U, 0U);
  CHECK_EQ(r.driver.SetChargerMode(static_cast<ChargerMode>(3)).error, Error::InvalidArgument);
  r.rig.Chip().env.thermal_shutdown = true;
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::ChargerFault);
  CHECK_EQ(r.rig.Chip().Peek(0x89) & 3U, 0U);
}

TEST_CASE("T17 charging with unsafe registers is refused whatever Configure did") {
  Ready r(BaseConfig());         // charger block not applied: the chip's own values stand
  r.rig.Chip().Poke(0x94, 0x98); // the option's 1.5 A input limit is above what the supply gives
  r.rig.Chip().Poke(0x8A, 0x00); // fast-charge timer disabled
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::Unsafe);
  r.rig.Chip().Poke(0x8A, 0x0A);
  r.rig.Chip().Poke(0x8F, 0x2F); // CHGCV 4.28 V, above the cell
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::OutOfLimits);
  r.rig.Chip().Poke(0x8F, 0x2B);
  r.rig.Chip().Poke(0x8E, 0x1F); // reserved CHG_CC code
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::OutOfLimits);
  r.rig.Chip().Poke(0x8E, 0x08);
  r.rig.Chip().Poke(0x94, 0xB8); // reserved VBUS_LIN_ILIM code
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::OutOfLimits);
  r.rig.Chip().Poke(0x94, 0x98);
  r.rig.Chip().Poke(0x92, 0x04); // thermistor ignored
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::Unsafe);
  r.rig.Chip().Poke(0x92, 0x05);
  r.rig.Chip().Poke(0x91, 0x00); // battery overcurrent protection off
  CHECK_EQ(r.driver.SetChargerMode(ChargerMode::Charge).error, Error::Unsafe);
  r.rig.Chip().Poke(0x91, 0x30);
  CHECK(r.driver.SetChargerMode(ChargerMode::Charge).ok());
}

TEST_CASE("T17 SetChargerMode(Charge) checks the as-found values and reports them") {
  Ready r;
  r.rig.Chip().Poke(0x8A, 0x00);
  const Result<ChargerAsFound> f = r.driver.CheckChargerAgainstLimits();
  CHECK(f.ok()); // the charger is not on: nothing to refuse yet
  CHECK(f.value.fast_timer == FastChargeTimer::Disabled);
  CHECK_EQ(f.value.charge_voltage_mv, 4200U);
  CHECK_EQ(f.value.input_limit_ma, 1500U);
}

TEST_CASE("T18 RestartCharger cycles off and on, a bounded number of times") {
  Ready r;
  REQUIRE(r.driver.Configure().ok());
  CHECK(r.driver.SetChargerMode(ChargerMode::Charge).ok());
  r.rig.Chip().env.timer_fault = true;
  CHECK_EQ(r.driver.RestartCharger().error, Error::ChargerFault); // the fault is still there
  CHECK_EQ(r.rig.Chip().Peek(0x89) & 3U, 0U);                     // but the charger was cycled off
  r.rig.Chip().env.timer_fault = false;
  CHECK(r.driver.RestartCharger().ok());
  CHECK_EQ(r.rig.Chip().Peek(0x89) & 3U, 2U);
  r.rig.Chip().env.timer_fault = true;
  CHECK_EQ(r.driver.RestartCharger().error, Error::ChargerFault);
  CHECK_EQ(r.driver.RestartCharger().error, Error::Refused); // three tries without charging: stop
  r.rig.Chip().env.timer_fault = false;
  r.rig.Chip().Poke(0x87, 0x01); // a charging state seen again clears the count
  CHECK(r.driver.ReadChargerStatus().ok());
  CHECK(r.driver.RestartCharger().ok());
}

TEST_CASE("T19 the watchdog is serviced by WDTCLR = 1 and the other bits are kept") {
  Config c = ChargerConfigOn();
  c.charger.watchdog = {true, WatchdogTime::S32};
  Ready r(c);
  REQUIRE(r.driver.Configure().ok());
  r.rig.Chip().env.watchdog_expired = true;
  CHECK(r.driver.ServiceChargerWatchdog().ok());
  CHECK_EQ(r.rig.Chip().wdt_clears, 1U);
  CHECK(LastWriteWas(r.rig, 0x91, 0xF1)); // WD_TIME 32 s, BATFET_OC, NOVBUS kept, WDTCLR = 01
  CHECK_EQ(r.rig.Chip().Peek(0x91) & 3U, 0U);
  CHECK(!r.rig.Chip().env.watchdog_expired);
  CHECK(kWatchdogServiceMaxS <= 16U);
}
