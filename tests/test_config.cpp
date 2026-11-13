/**
 * @file test_config.cpp
 * @brief Validate(): limits, grids, acknowledgements and enumerators, and the codecs. (T03 to T06.)
 */
#include "pf1550_codec.hpp"
#include "pf1550_config.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
Config Charging() {
  return ChargerConfigOn();
}
} // namespace

TEST_CASE("T03 Limits are mandatory and must be sane") {
  CHECK_EQ(Validate(Config{}), Error::InvalidArgument);
  Config c = BaseConfig();
  CHECK_EQ(Validate(c), Error::Ok);
  c.limits.cell_max_mv = 3400;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.limits.cell_max_mv = 4500;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.limits.cell_max_charge_ma = 50;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.limits.input_max_ma = 5;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.limits.rail[0] = {1200, 1000, 0}; // min above max
  CHECK_EQ(Validate(c), Error::InvalidArgument);
}

TEST_CASE("T03 charger values are on the grid and inside Limits") {
  Config c = Charging();
  CHECK_EQ(Validate(c), Error::Ok);
  c.charger.charge_voltage_mv = 4210; // off the 20 mV grid
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.charge_voltage_mv = 4240; // on the grid, above the cell
  CHECK_EQ(Validate(c), Error::OutOfLimits);
  c = Charging();
  c.charger.charge_current_ma = 525;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.charge_current_ma = 600;
  CHECK_EQ(Validate(c), Error::OutOfLimits);
  c = Charging();
  c.charger.input_limit_ma = 1500; // a table value above what the supply delivers
  CHECK_EQ(Validate(c), Error::OutOfLimits);
  c = Charging();
  c.charger.input_limit_ma = 450;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.termination_ma = 40;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.charge_voltage_mv = 0; // no default: it must be set
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.apply = false;
  c.charger.charge_voltage_mv = 0; // a charger left as found needs no values
  CHECK_EQ(Validate(c), Error::Ok);
}

TEST_CASE("T04 every safety downgrade needs its acknowledgement") {
  Config c = Charging();
  c.charger.fast_timer = FastChargeTimer::Disabled;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.allow_untimed_charge = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c = Charging();
  c.charger.thermistor = Thermistor::Ignore;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.battery_has_own_ntc_protection = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c = Charging();
  c.charger.batfet.oc_threshold = BatfetOc::Disabled;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.allow_batfet_oc_off = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c = Charging();
  c.charger.batfet.oc_keeps_closed = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c = Charging();
  c.charger.batfet.fault_policy.keep_closed_on_watchdog = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.allow_batfet_closed_on_fault = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c = Charging();
  c.charger.termination_ma = 10;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.eoc_accuracy_accepted = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c = Charging();
  c.charger.batfet.disable = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.allow_batfet_off = true;
  CHECK_EQ(Validate(c), Error::Ok);
}

TEST_CASE("T04 rails: windows, grids, DVS declaration, low power") {
  Config c = BaseConfig();
  c.sw1.apply = true;
  c.sw1.dvs = Dvs::Enabled;
  c.sw1.run_uv = FromMillivolts(1100);
  CHECK_EQ(Validate(c), Error::Ok);
  c.sw1.run_uv = FromMillivolts(1300); // inside the chip, outside the load's window
  CHECK_EQ(Validate(c), Error::OutOfLimits);
  c.sw1.run_uv = Microvolts{1103000}; // off the 12.5 mV grid
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c.sw1.run_uv = FromMillivolts(1100);
  c.sw1.dvs = Dvs::Disabled; // a voltage on a buck not declared DVS
  CHECK_EQ(Validate(c), Error::ReadOnlyRail);
  c = BaseConfig();
  c.sw3.apply = true;
  c.sw3.run_uv = FromMillivolts(1800); // SW3's voltage is never written
  CHECK_EQ(Validate(c), Error::ReadOnlyRail);
  c = BaseConfig();
  c.sw1.apply = true;
  c.sw1.modes.run = true;
  c.limits.rail[0] = {}; // enabling a rail whose load is not declared
  CHECK_EQ(Validate(c), Error::OutOfLimits);
  c = BaseConfig();
  c.ldo1.apply = true;
  c.ldo1.volt = FromMillivolts(1800);
  c.ldo1.modes.low_power = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.load_below_10ma = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c.ldo1.volt = FromMillivolts(1650); // between table entries
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.ldo2.apply = true;
  c.ldo2.volt = FromMillivolts(3400); // LDO2 stops at 3.3 V
  CHECK_EQ(Validate(c), Error::InvalidArgument);
}

TEST_CASE("T05 coin cell, power block and power-down order need their acknowledgements") {
  Config c = BaseConfig();
  c.coin.apply = true;
  c.coin.charge_enable = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.coin_cell_is_rechargeable = true;
  c.coin.voltage_mv = 2500;
  CHECK_EQ(Validate(c), Error::Ok);
  c.limits.coin_cell_max_mv = 2800; // the C40 rule: above 1.8 V the cell must tolerate 3.3 V
  CHECK_EQ(Validate(c), Error::OutOfLimits);
  c.limits.coin_cell_max_mv = 3300;
  c.coin.voltage_mv = 2550;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.power.apply = true;
  c.power.write_reset_paths = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.hard_reset_paths = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c.power.write_uvdet = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.undervoltage_threshold = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c = BaseConfig();
  c.power_down.apply = true;
  CHECK_EQ(Validate(c), Error::Unsafe);
  c.ack.power_down_order_reviewed = true;
  CHECK_EQ(Validate(c), Error::Ok);
  c.power_down.sequence.order[2] = 8;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
}

TEST_CASE("T06 an enumerator that is not one is rejected") {
  Config c = Charging();
  c.charger.fast_timer = static_cast<FastChargeTimer>(9);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.vbus.dpm = static_cast<DpmVoltage>(8);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = Charging();
  c.charger.jeita.cv_adjust = static_cast<CvAdjust>(4);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.as_found_policy = static_cast<AsFoundPolicy>(2);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.rail_order[0] = c.rail_order[1]; // not a permutation
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.led.apply = true;
  c.led.duty_32nds = 33;
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.sw1.apply = true;
  c.sw1.current_limit = static_cast<SwCurrentLimit>(4);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.power.apply = true;
  c.power.uvdet = static_cast<Uvdet>(4);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
  c = BaseConfig();
  c.usbphy.apply = true;
  c.usbphy.voltage = static_cast<UsbPhyVoltage>(2);
  CHECK_EQ(Validate(c), Error::InvalidArgument);
}

TEST_CASE("T06 codecs: grids, anchors and reserved codes") {
  CHECK_EQ(ChargeVoltageCode(3500).value, 8);
  CHECK_EQ(ChargeVoltageCode(4440).value, 55);
  CHECK(!ChargeVoltageCode(3480).ok());
  CHECK_EQ(ChargeVoltageMv(0), 3500U);
  CHECK_EQ(ChargeVoltageMv(9), 3520U);
  CHECK_EQ(ChargeVoltageMv(60), 4440U);
  for (uint32_t ma = 100; ma <= 1000; ma += 50) {
    CHECK_EQ(ChargeCurrentMa(ChargeCurrentCode(ma).value).value, ma);
  }
  CHECK(!ChargeCurrentMa(19).ok());
  for (uint32_t code = 0; code < 21; ++code) {
    CHECK_EQ(InputLimitCode(InputLimitMa(code).value).value, code);
  }
  CHECK(!InputLimitMa(21).ok());
  CHECK(!TerminationMa(5).ok());
  CHECK_EQ(TerminationMa(2).value, 20U);
  CHECK_EQ(BuckDvsUv(BuckDvsCode(1387500).value), 1387500U);
  CHECK_EQ(BuckFixedUvOf(0), 1100000U);
  CHECK_EQ(BuckFixedUvOf(6), 3000000U);
  CHECK_EQ(BuckFixedUvOf(40), 3300000U);
  CHECK_EQ(Sw3Uv(3), 2100000U);
  CHECK_EQ(LdoUv(Rail::Ldo1, 5), 1000000U);
  CHECK_EQ(LdoUv(Rail::Ldo2, 7), 2500000U);
  CHECK_EQ(LdoUv(Rail::Ldo3, 26), 2800000U);
  CHECK_EQ(LdoUv(Rail::VrefDdr, 1), 0U);
  CHECK_EQ(CoinVoltageCode(1800).value, 0);
  CHECK(DecodePowerState(0x0D) == PowerState::Standby);
  CHECK((DecodeOption(0) == Option::A0) && (DecodeOption(3) == Option::A3) &&
        (DecodeOption(4) == Option::Unknown));
}
