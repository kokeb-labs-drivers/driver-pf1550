/**
 * @file test_configure.cpp
 * @brief Configure(): what is written, in what order, with what verification. (T07 to T14.)
 */
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {

std::size_t IndexOfFirstWrite(Rig& r, uint8_t address) {
  for (std::size_t i = 0; i < r.Chip().writes.size(); ++i) {
    if (r.Chip().writes[i].address == address) {
      return i;
    }
  }
  return r.Chip().writes.size();
}

std::size_t IndexOfLastWrite(Rig& r, uint8_t address) {
  for (std::size_t i = r.Chip().writes.size(); i > 0; --i) {
    if (r.Chip().writes[i - 1].address == address) {
      return i - 1;
    }
  }
  return r.Chip().writes.size();
}

} // namespace

TEST_CASE("T07 charger block writes the documented bytes and leaves the mode last") {
  Config c = ChargerConfigOn();
  c.charger.mode = ChargerMode::Charge;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  rig.Chip().env.vbus_valid = true;
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x8A), 0x0A);    // FCHGTIME 4 h, EOCTIME 10 min, TPRECHG 30 min
  CHECK_EQ(rig.Chip().Peek(0x8D), 0x41);    // CHG_RESTART 150 mV, IEOC 50 mA
  CHECK_EQ(rig.Chip().Peek(0x8E), 0x08);    // CHG_CC 500 mA
  CHECK_EQ(rig.Chip().Peek(0x8F), 0xAB);    // CHGCV 4.20 V, VSYSMIN 4.3 V
  CHECK_EQ(rig.Chip().Peek(0x91), 0xB0);    // BATFET_OC 3.2 A, protected without VBUS
  CHECK_EQ(rig.Chip().Peek(0x92), 0x85);    // THM_CNFG suspend, REGTEMP 95 C, foldback on
  CHECK_EQ(rig.Chip().Peek(0x94), 0x98);    // VBUS_LIN_ILIM 1000 mA
  CHECK_EQ(rig.Chip().Peek(0x89) & 3U, 2U); // charging
  // The mode is parked before the first parameter and set after the last.
  CHECK(IndexOfFirstWrite(rig, 0x89) < IndexOfFirstWrite(rig, 0x8A));
  CHECK(IndexOfLastWrite(rig, 0x89) > IndexOfLastWrite(rig, 0x9D));
  CHECK(IndexOfLastWrite(rig, 0x89) > IndexOfLastWrite(rig, 0x8F));
  CHECK_EQ(rig.Chip().protected_writes, 0U);
}

TEST_CASE("T07 charger mode LinearOnly leaves charging off") {
  Rig rig(2, ChargerConfigOn());
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x89) & 3U, 1U);
}

TEST_CASE("T07 reserved bits and fields the block does not own are preserved") {
  Rig rig(2, ChargerConfigOn());
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  rig.Chip().Poke(0x8E, 0x60); // PRECHGLB_THRS bits
  rig.Chip().Poke(0x91, 0x33); // WDTCLR bits and BATFET_OC
  rig.Chip().Poke(0x9B, 0xF8); // reserved bits above the VBUS2SYS fields
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x8E), 0x68);
  CHECK_EQ(rig.Chip().Peek(0x9B), 0xF9);
}

TEST_CASE("T07 JEITA, DPM, debounce and interrupt options reach their fields") {
  Config c = ChargerConfigOn();
  c.charger.thermistor = Thermistor::Jeita2;
  c.charger.thermal_reg = ThermalRegTemp::C110;
  c.charger.jeita = {Cold::CMinus10, Hot::C55,        Cool::C10,
                     Warm::C50,      CvAdjust::Mv200, CcAdjust::Pct100};
  c.charger.vbus.dpm = DpmVoltage::Mv4500;
  c.charger.vbus.dpm_stop = DpmStop::Mv250;
  c.charger.vbus.ovlo_debounce = ShortDebounce::Ms10;
  c.charger.vbus.vbus2sys_debounce = ShortDebounce::Ms1;
  c.charger.vbus.vbus2sys_threshold = Vbus2SysThreshold::Mv175;
  c.charger.vbus.sys_wakeup_delay = WakeupDelay::Ms100;
  c.charger.interrupts = {true, true};
  c.charger.precharge_timer = PrechargeTimer::Min45;
  c.charger.eoc_timer = EocTimer::Min70;
  c.charger.fast_timer = FastChargeTimer::Hours14;
  c.charger.restart = Restart::Disabled;
  c.charger.watchdog = {true, WatchdogTime::S32};
  c.charger.batfet.oc_threshold = BatfetOc::A2p2;
  c.charger.batfet.oc_without_vbus = true;
  c.charger.termination_ma = 20;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x8A), 0xBF);        // TPRECHG 45 min, EOCTIME 7, FCHGTIME 7
  CHECK_EQ(rig.Chip().Peek(0x8D), 0x23);        // IEOC 20 mA (code 2), CHG_RESTART disabled
  CHECK_EQ(rig.Chip().Peek(0x91), 0xD0);        // BOVRC_NOVBUS, WD_TIME 32 s, BATFET_OC 2.2 A
  CHECK_EQ(rig.Chip().Peek(0x92), 0xBB);        // JEITA 2, 110 C, cold -10, hot 55, foldback
  CHECK_EQ(rig.Chip().Peek(0x95), 0x26);        // VBUS_DPM_REG 4.5 V, VIN_DPM_STOP 250 mV
  CHECK_EQ(rig.Chip().Peek(0x98), 0x33);        // SYS_WKUP_DLY 100 ms, VBUS_OV_TDB 10 ms
  CHECK_EQ(rig.Chip().Peek(0x99), 0x03);        // CHG_INT_GEN, EOC_INT
  CHECK_EQ(rig.Chip().Peek(0x9A), 0x3F);        // warm 50, cool 10, CV_ADJ 200 mV, CC_ADJ 100 %
  CHECK_EQ(rig.Chip().Peek(0x9B), 0x06);        // VBUS2SYS_TDB 1 ms, threshold 175 mV
  CHECK((rig.Chip().Peek(0x89) & 0x08U) != 0U); // WDTEN
}

TEST_CASE("T08 the BATFET fault policy and DISBATFET are written when acknowledged") {
  Config c = ChargerConfigOn();
  c.charger.batfet.fault_policy = {true, true, true, true, true};
  c.charger.batfet.disable = true;
  c.ack.allow_batfet_closed_on_fault = true;
  c.ack.allow_batfet_off = true;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x9D), 0x1F);
  CHECK((rig.Chip().Peek(0x89) & 0x10U) != 0U);
}

TEST_CASE("T09 buck and LDO blocks: enables, voltages, current limit, forced PWM") {
  Config c = BaseConfig();
  c.sw1 = {true,
           Dvs::Enabled,
           FromMillivolts(1100),
           FromMillivolts(1000),
           FromMillivolts(1000),
           {true, true, false, false}};
  c.sw1.current_limit = SwCurrentLimit::A1p5;
  c.sw1.fpwm = true;
  c.sw1.dvs_speed = DvsSpeed::Us4;
  c.sw1.output_discharge = false;
  c.ldo1 = {true, FromMillivolts(1800), {true, false, false, false}};
  c.ldo2 = {true, FromMillivolts(3300), {true, true, true, false}};
  c.vrefddr = {true, {true, true, false, false}};
  c.vsnvs = {true, true};
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x32), 40); // 1.1 V: code 40
  CHECK_EQ(rig.Chip().Peek(0x33), 32); // 1.0 V: code 32
  CHECK_EQ(rig.Chip().Peek(0x34), 32);
  CHECK_EQ(rig.Chip().Peek(0x35), 0xD3); // RDIS_ENB, FPWM, DVSSPEED 4 us, EN, STBY_EN
  CHECK_EQ(rig.Chip().Peek(0x36), 2);    // ILIM 1.5 A
  CHECK_EQ(rig.Chip().Peek(0x4C), 16);   // LDO1 1.8 V
  CHECK_EQ(rig.Chip().Peek(0x4D), 0x01);
  CHECK_EQ(rig.Chip().Peek(0x4F), 15); // LDO2 3.3 V
  CHECK_EQ(rig.Chip().Peek(0x50), 0x07);
  CHECK_EQ(rig.Chip().Peek(0x4A), 0x03);
  CHECK_EQ(rig.Chip().Peek(0x48) & 0x20U, 0x20U); // LIBGDIS
  CHECK_EQ(rig.Chip().protected_writes, 0U);
  CHECK(!WroteTo(rig, 0x3E)); // SW3 voltages are never written
}

TEST_CASE("T09 the rail order is the caller's") {
  Config c = BaseConfig();
  c.sw1.apply = true;
  c.sw1.dvs = Dvs::Enabled; // A2 is a DVS part for SW1 (Table 85)
  c.ldo1.apply = true;
  c.rail_order = {Rail::Ldo1, Rail::Sw1,  Rail::Sw2,    Rail::Sw3,
                  Rail::Ldo2, Rail::Ldo3, Rail::VrefDdr};
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK(IndexOfFirstWrite(rig, 0x4D) < IndexOfFirstWrite(rig, 0x35));
}

TEST_CASE("T09 a DVS step waits for the done flag, and an unchanged voltage does not") {
  Config c = BaseConfig();
  c.sw1 = {true, Dvs::Enabled, FromMillivolts(1100)};
  c.sw1.modes.run = true;
  c.ack.dvs_on_unlisted_part = true;
  c.as_found_policy =
      AsFoundPolicy::MakeSafe; // option A4 starts with the charger on, no timer, 1.5 A
  Rig rig(4, c);               // A4: SW1 at 1.1 V (code 40), enabled
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.port.delayed_us, 0U); // same voltage: no ramp, no wait
  REQUIRE(d.SetRailVoltage(Rail::Sw1, FromMillivolts(1200)).ok());
  CHECK(rig.port.delayed_us >= 100U);
  CHECK(rig.port.delayed_us <= kDvsWaitMaxUs);
  CHECK_EQ(rig.Chip().Peek(0x32), 48);
  CHECK_EQ(rig.Chip().Peek(0x0E) & 1U, 0U); // the done flag was cleared by the driver
}

TEST_CASE("T09 a DVS step that never finishes times out") {
  Config c = BaseConfig();
  c.sw1 = {true, Dvs::Enabled, FromMillivolts(1100)};
  c.ack.dvs_on_unlisted_part = true;
  c.as_found_policy = AsFoundPolicy::MakeSafe;
  Rig rig(4, c);
  rig.Chip().readings.dvs_never_done = true;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.SetRailVoltage(Rail::Sw1, FromMillivolts(1200)).error, Error::Timeout);
  CHECK(rig.port.delayed_us >= kDvsWaitMaxUs);
}

TEST_CASE("T10 coin cell, USB PHY, LED and power control reach their registers") {
  Config c = BaseConfig();
  c.ack.coin_cell_is_rechargeable = true;
  c.coin = {true, true, 3300};
  c.usbphy = {true, true, UsbPhyVoltage::V4p9, false};
  c.led = {true, true, 16, true, LedFrequency::Hz256, LedMode::FlashWhileCharging, true};
  c.power.apply = true;
  c.power.write_standby = c.power.write_debounce = c.power.write_regscp = true;
  c.power.write_low_sys_warn = true;
  c.power.standby_delay = StandbyDelay::Cycles3;
  c.power.standby_inverted = true;
  c.power.pwron_debounce = Debounce::Ms125;
  c.power.onkey_debounce = Debounce::Ms750;
  c.power.ldo_off_on_current_limit = true;
  c.power.low_sys_warn = LowSysWarn::Mv3700;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x30), 0x1F);
  CHECK_EQ(rig.Chip().CoinChargeMv(), 3300U);
  CHECK_EQ(rig.Chip().Peek(0x96), 0x06);    // USBPHYLDO, 4.9 V, no active discharge
  CHECK_EQ(rig.Chip().Peek(0x9C), 0xD0);    // LED_EN, LED_RAMP, duty 16
  CHECK_EQ(rig.Chip().Peek(0x9E), 0x36);    // LEDOVRD, LED_CFG, 6 mA, 256 Hz
  CHECK_EQ(rig.Chip().Peek(0x58) & 7U, 7U); // STANDBYDLY 3, STANDBYINV
  CHECK_EQ(rig.Chip().Peek(0x59),
           0xCE); // reset path untouched (0x80), REGSCPEN, ONKEY 750, PWRON 125
  CHECK_EQ(static_cast<uint32_t>(rig.Chip().Peek(0x5A)) >> 2U, 2U); // LOW_SYS_WARN 3.7 V
}

TEST_CASE("T10 reset paths and UVDET are written only when asked and acknowledged") {
  Config c = BaseConfig();
  c.ack.hard_reset_paths = true;
  c.ack.undervoltage_threshold = true;
  c.power.apply = true;
  c.power.write_reset_paths = true;
  c.power.write_uvdet = true;
  c.power.pwron_long_press_reset = true;
  c.power.restart_on_long_press = true;
  c.power.onkey_reset = false;
  c.power.global_reset = GlobalReset::S12;
  c.power.uvdet = Uvdet::Mv3100;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x59) & 0xF0U, 0x30U);
  CHECK_EQ(static_cast<uint32_t>(rig.Chip().Peek(0x58)) >> 6U, 2U);
  CHECK_EQ(rig.Chip().Peek(0x5A) & 3U, 3U);
  Config plain = BaseConfig();
  plain.power.apply = true;
  Rig rig2(2, plain);
  Driver d2(rig2.port, rig2.cfg);
  REQUIRE(d2.EnsureInitialized().ok());
  REQUIRE(d2.Configure().ok());
  CHECK_EQ(rig2.Chip().Peek(0x59) & 0xF0U, 0x80U); // reset paths as found
  CHECK_EQ(rig2.Chip().Peek(0x5A) & 3U, 2U);       // UVDET as found
  // With no `write_...` flag set, `apply` writes none of the three power control registers.
  CHECK(!WroteTo(rig2, 0x58) && !WroteTo(rig2, 0x59) && !WroteTo(rig2, 0x5A));
}

TEST_CASE(
    "m8 BOVRC_NOVBUS defaults to protected; off needs the battery overcurrent acknowledgement") {
  Config c = ChargerConfigOn();
  CHECK(c.charger.batfet.oc_without_vbus);
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x91) & 0x80U, 0x80U);
  Config off = ChargerConfigOn();
  off.charger.batfet.oc_without_vbus = false;
  CHECK_EQ(Validate(off), Error::Unsafe);
  off.ack.allow_batfet_oc_off = true;
  CHECK_EQ(Validate(off), Error::Ok);
}

TEST_CASE("m7 standby or sleep without run is rejected: EN gates both (Table 42)") {
  Rig rig(2);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.SetRailModes(Rail::Ldo1, {false, true, false, false}).error, Error::InvalidArgument);
  CHECK_EQ(d.SetRailModes(Rail::Ldo1, {false, false, true, false}).error, Error::InvalidArgument);
  CHECK(d.SetRailModes(Rail::Ldo1, {true, true, true, false}).ok());
}

TEST_CASE("T11 Configure refuses charging while the charger reports a fault") {
  Config c = ChargerConfigOn();
  c.charger.mode = ChargerMode::Charge;
  Rig rig(2, c);
  rig.Chip().env.vbus_valid = true;
  rig.Chip().env.timer_fault = true;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.Configure().error, Error::ChargerFault);
  CHECK_EQ(rig.Chip().Peek(0x89) & 3U, 1U); // parameters written, charging not enabled
  CHECK_EQ(rig.Chip().Peek(0x8F), 0xAB);
}

TEST_CASE("T12 a write that does not hold is reported") {
  Config c = BaseConfig();
  c.ldo1 = {true, Microvolts{}, {false, false, false, false}};
  Rig rig(2, c);
  rig.Chip().readings.rw1s_set_only = true; // a written 0 does not clear an OTP-loaded bit (C4)
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  CHECK_EQ(d.Configure().error, Error::VerifyFailed);
}

TEST_CASE("T12 a bus error in the middle of Configure stops it") {
  Config c = ChargerConfigOn();
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  rig.port.state.fail_from = rig.port.state.transfers + 6;
  CHECK_EQ(d.Configure().error, Error::Bus);
  rig.port.state.fail_from = -1;
  CHECK(d.Configure().ok()); // the same call succeeds once the bus recovers
}

TEST_CASE("T13 no write reaches a register that is not meant to be written") {
  Config c = ChargerConfigOn();
  c.sw1 = {true,
           Dvs::Enabled,
           FromMillivolts(1100),
           FromMillivolts(1000),
           FromMillivolts(1000),
           {true, true, true, false}};
  c.sw2 = {true,         Dvs::Disabled, Microvolts{},
           Microvolts{}, Microvolts{},  {true, true, false, false}};
  c.sw3 = {true,         Dvs::Disabled, Microvolts{},
           Microvolts{}, Microvolts{},  {true, true, false, false}};
  c.ldo1 = {true, FromMillivolts(1800), {true, true, false, false}};
  c.ldo2 = {true, FromMillivolts(3300), {true, true, false, false}};
  c.ldo3 = {true, FromMillivolts(3300), {true, true, false, false}};
  c.vrefddr = {true, {true, true, false, false}};
  c.vsnvs = {true, false};
  c.apply_irq_mask = true;
  c.power.apply = true;
  c.ack.dvs_on_unlisted_part = true;
  // SW2 is left at the 3.3 V this option starts it at (a held voltage must lie inside the window).
  c.limits.rail[static_cast<std::size_t>(Rail::Sw2)] = {1100, 3300, 800};
  c.as_found_policy = AsFoundPolicy::MakeSafe;
  Rig rig(4, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().protected_writes, 0U);
  for (const WriteRecord& w : rig.Chip().writes) {
    CHECK(reg::IsWritable(w.address));
    CHECK((w.address != reg::kKey1) && (w.address != reg::kChgrKey2) &&
          (w.address != reg::kRc16Mhz));
  }
}

TEST_CASE("T14 the interrupt mask block writes defined bits and preserves reserved ones") {
  Config c = BaseConfig();
  c.apply_irq_mask = true;
  c.irq_mask.masked = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFF};
  Rig rig(2, c);
  rig.Chip().Poke(0x25, 0xC0); // reserved bits of ONKEY_INT_MASK0 set, defined bits clear
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  CHECK_EQ(rig.Chip().Peek(0x82), 0x10); // bit 4 is not a source: preserved as the chip had it
  CHECK_EQ(rig.Chip().Peek(0x25), 0xFE); // the other defined bits masked, reserved bits as found
}
