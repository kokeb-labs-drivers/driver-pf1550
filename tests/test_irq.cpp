/**
 * @file test_irq.cpp
 * @brief Interrupts: masks, categories, status service and clear, sense, INTB. (T31 to T35.)
 */
#include "pf1550_irq.hpp"
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

void Unmask(Ready& r) {
  CHECK(r.driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok()); // INTB follows the sources
}
} // namespace

TEST_CASE("ReadRegister reads one documented register and nothing else") {
  Ready r;
  const std::size_t before = r.rig.Chip().writes.size();
  CHECK_EQ(r.driver.ReadRegister(0x00).value, r.rig.Chip().Peek(0x00));
  CHECK_EQ(r.driver.ReadRegister(0x70).error, Error::InvalidArgument);
  CHECK_EQ(r.rig.Chip().writes.size(), before);
}

TEST_CASE("T31 the source map agrees with the register tables") {
  CHECK_EQ(kIrqLocations.size(), kIrqSourceCount);
  CHECK((LocationOf(IrqSource::ChgSupplement).group == IrqGroup::Charger) &&
        (LocationOf(IrqSource::ChgSupplement).bit == 0));
  CHECK_EQ(LocationOf(IrqSource::ChgThermistor).bit, 7);
  CHECK(LocationOf(IrqSource::Sw3LowSideLimit).group == IrqGroup::SwLowSide);
  CHECK(LocationOf(IrqSource::Sw2HighSideLimit).group == IrqGroup::SwHighSide);
  CHECK_EQ(LocationOf(IrqSource::Sw2DvsDone).bit, 1);
  CHECK_EQ(LocationOf(IrqSource::Onkey8s).bit, 5);
  CHECK_EQ(LocationOf(IrqSource::LowSysWarning).bit, 3);
  CHECK_EQ(kIrqStatRegs[static_cast<std::size_t>(IrqGroup::Onkey)], 0x24);
  CHECK_EQ(kIrqSenseRegs[0], 0x84);
  std::size_t faults = 0;
  for (std::size_t i = 0; i < kIrqSourceCount; ++i) {
    faults += IsFaultSource(static_cast<IrqSource>(i)) ? 1U : 0U;
  }
  CHECK_EQ(faults, 11U);
  CHECK(!IsFaultSource(static_cast<IrqSource>(40)));
  CHECK(LocationOf(static_cast<IrqSource>(40)).group == IrqGroup::Charger);
}

TEST_CASE("T32 SetIrqMask: the power-up state masks everything, UnmaskOnly opens named sources") {
  Ready r;
  CHECK_EQ(r.rig.Chip().Peek(0x29), 0xFF);
  const IrqMask m = UnmaskOnly(
      std::array<IrqSource, 3>{IrqSource::PowerUp, IrqSource::Ldo2Fault, IrqSource::ChgVbus});
  REQUIRE(r.driver.SetIrqMask(m).ok());
  CHECK_EQ(r.rig.Chip().Peek(0x29) & 0x1FU, 0x1EU);
  CHECK_EQ(r.rig.Chip().Peek(0x19) & 0x07U, 0x05U);
  CHECK_EQ(r.rig.Chip().Peek(0x82) & 0xEFU, 0xCFU);
  CHECK(!r.rig.Chip().IntbAsserted());
  r.rig.Chip().Raise(0x28, 0);
  CHECK(r.rig.Chip().IntbAsserted());
}

TEST_CASE("T33 ServiceInterrupts copies the status out, then clears exactly those bits") {
  Ready r;
  Unmask(r);
  r.rig.Chip().Raise(0x28, 1); // PWRDN_I
  r.rig.Chip().Raise(0x24, 3); // 3SI
  r.rig.Chip().Raise(0x18, 1); // LDO2_FAULTI
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK(Has(e.value, IrqSource::PowerDown) && Has(e.value, IrqSource::Onkey3s) &&
        Has(e.value, IrqSource::Ldo2Fault));
  CHECK(!Has(e.value, IrqSource::OnkeyPush));
  CHECK(e.value.fault_class);
  CHECK_EQ(e.value.categories, 0xB0);
  CHECK_EQ(e.value.passes, 1);
  CHECK_EQ(r.rig.Chip().Peek(0x28), 0);
  CHECK_EQ(r.rig.Chip().Peek(0x06), 0);
  const Result<InterruptEvents> again = r.driver.ServiceInterrupts();
  CHECK(again.ok());
  CHECK_EQ(again.value.passes, 0);
  CHECK(!again.value.fault_class);
}

TEST_CASE("T33 clear modes: write-one, on-read and both leave no bit set and lose none") {
  for (const ClearMode mode : {ClearMode::WriteOne, ClearMode::OnRead, ClearMode::Both}) {
    Ready r;
    Unmask(r);
    r.rig.Chip().readings.int_clear = mode;
    r.rig.Chip().Raise(0x08, 0);
    r.rig.Chip().Raise(0x80, 5);
    const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
    REQUIRE(e.ok());
    CHECK(Has(e.value, IrqSource::Sw1LowSideLimit));
    CHECK(Has(e.value, IrqSource::ChgVbus));
    CHECK_EQ(r.rig.Chip().Peek(0x08), 0);
    CHECK_EQ(r.rig.Chip().Peek(0x80), 0);
  }
}

TEST_CASE("T33 an event that arrives during the service is picked up in a later pass") {
  Ready r;
  Unmask(r);
  r.rig.Chip().Raise(0x2A - 2, 4); // SYS_OVLO_I
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  CHECK(e.value.passes >= 1);
  CHECK(Has(e.value, IrqSource::SysOvervoltage));
  CHECK(e.value.fault_class);
}

TEST_CASE("T34 INTB: an idle line costs no bus traffic") {
  Ready r;
  const int before = r.rig.port.state.transfers;
  const Result<InterruptEvents> idle = r.driver.ServiceInterrupts();
  CHECK(idle.ok());
  // The mask is not known to the driver: a source may be hidden from INTB, so the status is read.
  CHECK(r.rig.port.state.transfers > before);
  Ready u;
  REQUIRE(u.driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  const int t0 = u.rig.port.state.transfers;
  CHECK(u.driver.ServiceInterrupts().ok());
  CHECK_EQ(u.rig.port.state.transfers, t0); // INTB high: nothing is read
  CHECK(u.rig.port.gpio_gets > 0);
  u.rig.Chip().Raise(0x28, 1);
  const Result<bool> asserted = u.driver.IntbAsserted();
  CHECK(asserted.ok() && asserted.value);
  CHECK(Has(u.driver.ServiceInterrupts().value, IrqSource::PowerDown));
}

TEST_CASE("T34 a failing INTB read falls back to reading the chip") {
  Ready r;
  r.rig.port.gpio_ok = false;
  r.rig.Chip().Raise(0x28, 2);
  const Result<InterruptEvents> e = r.driver.ServiceInterrupts();
  CHECK(Has(e.value, IrqSource::PwronChange));
  CHECK_EQ(r.driver.IntbAsserted().error, Error::Bus);
}

TEST_CASE("T35 categories and sense registers") {
  Ready r;
  r.rig.Chip().Raise(0x0B, 2); // SW3_HS_I
  r.rig.Chip().Raise(0x0E, 0); // SW1_DVS_DONE_I
  Result<Categories> c = r.driver.ReadCategories();
  REQUIRE(c.ok());
  CHECK(c.value.sw1 && c.value.sw3 && !c.value.sw2 && !c.value.charger && !c.value.misc);
  r.rig.Chip().env.onkey_pushed = true;
  r.rig.Chip().env.pwron_high = false;
  CHECK_EQ(r.driver.ReadSense(IrqGroup::Onkey).value & 1U, 1U);
  CHECK_EQ(r.driver.ReadSense(IrqGroup::Misc).value & 4U, 0U);
  r.rig.Chip().env.vbus_valid = true;
  CHECK((r.driver.ReadSense(IrqGroup::Charger).value & 0x20U) != 0U);
  CHECK_EQ(r.driver.ReadSense(static_cast<IrqGroup>(9)).error, Error::InvalidArgument);
}

TEST_CASE("T35 the snapshot is read only and complete") {
  Ready r;
  const std::size_t writes = WriteCount(r.rig);
  const Result<Snapshot> s = r.driver.ReadSnapshot();
  REQUIRE(s.ok());
  CHECK_EQ(WriteCount(r.rig), writes);
  CHECK_EQ(s.value.variant.device_id, 0x7C);
  CHECK(s.value.state == PowerState::Run);
  CHECK(s.value.rails[0].enabled_run);
  CHECK_EQ(s.value.power_down.order[2], 1);
  CHECK_EQ(s.value.por_delay_code, 0);
}

TEST_CASE("T35 SelfCheck reports identity, state, rails and latched faults") {
  Config c = BaseConfig();
  c.ldo1 = {true, Microvolts{}, {true, true, false, false}};
  Ready r(c);
  Result<SelfCheckReport> s = r.driver.SelfCheck();
  REQUIRE(s.ok());
  CHECK(s.value.identity_ok && s.value.state_run && s.value.rails_as_configured &&
        s.value.no_latched_fault);
  r.rig.Chip().Raise(0x18, 0);
  s = r.driver.SelfCheck();
  CHECK(!s.value.no_latched_fault);
  r.rig.Chip().Poke(0x4D, 0x00);
  r.rig.Chip().SetState(0x0D);
  s = r.driver.SelfCheck();
  CHECK(!s.value.rails_as_configured && !s.value.state_run);
  r.rig.port.state.fail_from = r.rig.port.state.transfers + 1;
  CHECK_EQ(r.driver.SelfCheck().error, Error::Bus);
}
