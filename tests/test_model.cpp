/**
 * @file test_model.cpp
 * @brief The register table of the driver against the independent chip model, and the model against
 * the data sheet's reset values. (T36.)
 */
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

TEST_CASE("T36 the access table has 90 unique registers and agrees with the model") {
  CHECK_EQ(reg::kRegisters.size(), 90U);
  for (std::size_t i = 0; i < reg::kRegisters.size(); ++i) {
    for (std::size_t j = i + 1; j < reg::kRegisters.size(); ++j) {
      CHECK(reg::kRegisters[i].address != reg::kRegisters[j].address);
    }
  }
  for (unsigned a = 0; a < 0xA0; ++a) {
    Pf1550Model chip(2);
    chip.Write(static_cast<uint8_t>(a), 0x00);
    const bool protected_hit = chip.protected_writes != 0U;
    const auto addr = static_cast<uint8_t>(a);
    if (!reg::IsDocumented(addr)) {
      CHECK(protected_hit); // an undocumented register is protected in the model too
      CHECK(!reg::IsWritable(addr));
    }
    if (reg::IsWritable(addr)) {
      CHECK(!protected_hit); // nothing the driver may write is a protected register of the chip
    }
  }
  CHECK(!reg::IsWritable(reg::kKey1) && !reg::IsWritable(reg::kChgrKey2) &&
        !reg::IsWritable(reg::kRc16Mhz));
  CHECK(!reg::IsWritable(reg::kSw3Volt) && !reg::IsWritable(reg::kSw3StbyVolt) &&
        !reg::IsWritable(reg::kSw3SlpVolt));
}

TEST_CASE("T36 the model resets to the data sheet values of Table 85 for each option") {
  const uint8_t volt1[] = {0, 0, 32, 63, 40, 63, 54, 63, 63, 7};
  for (uint8_t o = 1; o <= 9; ++o) {
    Pf1550Model chip(o);
    CHECK_EQ(chip.Read(0x00), 0x7C);
    CHECK_EQ(chip.Read(0x01), o);
    CHECK_EQ(chip.Read(0x32), volt1[o]);
    CHECK_EQ(chip.Read(0x8F) & 0x3FU, 43); // CV 4.2 V
    CHECK_EQ(chip.Read(0x89) & 3U, ((o == 2) || (o == 6) || (o == 9)) ? 1U : 2U);
    CHECK_EQ(chip.Read(0x8A), 0x00); // fast-charge timer disabled, EOCTIME 16 s, TPRECHG 30 min
    CHECK_EQ(chip.Read(0x94) >> 3, o == 1 ? 14 : 20);
    CHECK_EQ(chip.Read(0x95) & 7U, o == 4 ? 6U : 0U);
    CHECK_EQ(chip.Read(0x8E) & 0x1FU, ((o >= 3) && (o <= 8)) ? 8U : 0U);
    const uint32_t expect_sel = ((o == 6) || (o == 9)) ? 1U : 2U;
    CHECK_EQ(static_cast<uint32_t>(chip.Read(0x8F)) >> 6U, expect_sel);
    CHECK_EQ(chip.Read(0x5A) & 3U, 2U);                         // UVDET rising 3.0 V
    CHECK_EQ(static_cast<uint32_t>(chip.Read(0x58)) >> 6U, 0U); // 4 s global reset
  }
}

TEST_CASE("T36 the model's status registers clear on a written one and not on a written zero") {
  Pf1550Model chip(2);
  chip.Raise(0x28, 0);
  chip.Raise(0x28, 3);
  chip.Write(0x28, 0x00);
  CHECK_EQ(chip.Read(0x28), 0x09);
  chip.Write(0x28, 0x01);
  CHECK_EQ(chip.Read(0x28), 0x08);
  chip.Write(0x2A, 0xFF); // SENSE registers are read only
  CHECK_EQ(chip.protected_writes, 1U);
}

TEST_CASE("T36 the model's observers show what Configure wrote (charger, USB PHY, LED)") {
  Config c = ChargerConfigOn();
  c.sw1.dvs = Dvs::Enabled;
  c.usbphy = {true, true, UsbPhyVoltage::V4p9, false};
  c.led.apply = true;
  c.led.enable = true;
  c.led.duty_32nds = 16;
  c.led.mode = LedMode::FlashWhileCharging;
  Rig rig(2, c);
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  REQUIRE(d.Configure().ok());
  const Pf1550Model& chip = rig.Chip();
  CHECK_EQ(chip.AppliedCvMv(), 4200U);
  CHECK_EQ(chip.AppliedCcMa(), 500U);
  CHECK(chip.UsbphyEnabled());
  CHECK_EQ(chip.UsbphyMv(), 4900U);
  CHECK(chip.Led().enabled);
  CHECK(chip.Led().flash_while_charging);
  CHECK_EQ(chip.Led().duty_32nds, 16U);
  CHECK(!chip.Led().software_override);
  // The model's own decoding agrees with the driver's codecs.
  CHECK_EQ(chip.AppliedCvMv(), ChargeVoltageMv(reg::Chgcv::Extract(chip.Peek(0x8F))));
}
