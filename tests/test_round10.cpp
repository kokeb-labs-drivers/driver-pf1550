/**
 * @file test_round10.cpp
 * @brief The ninth review round: a charger status byte that `ReadRegister()` read (and, on a chip
 * that clears on read, cleared) still owes the input limit check, so the next
 * `ServiceInterrupts()` restores a limit that went back to its OTP value.
 */
#include <cstdint>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
constexpr uint8_t kChgInt = 0x80;
constexpr uint8_t kVbusLim = 0x94;

constexpr uint8_t LimitByte(uint32_t ma) {
  return static_cast<uint8_t>(InputLimitCode(ma).value << 3U);
}

Config Limit500Config() {
  Config c = ChargerConfigOn();
  c.charger.mode = ChargerMode::LinearOnly;
  c.charger.input_limit_ma = 500;
  return c;
}

void CheckAfterRawRead(ClearMode mode) {
  Rig rig(2, Limit500Config());
  Driver driver(rig.port, rig.cfg);
  REQUIRE(driver.EnsureInitialized().ok());
  REQUIRE(driver.Configure().ok());
  REQUIRE(driver.SetChargerMode(ChargerMode::Charge).ok());
  rig.Chip().env.vbus_valid = true;
  rig.Chip().readings.int_clear = mode;
  REQUIRE(driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  rig.Chip().VbusRemoved();
  rig.Chip().Raise(kChgInt, 5);
  const Result<uint8_t> raw = driver.ReadRegister(kChgInt);
  REQUIRE(raw.ok());
  CHECK_EQ(raw.value, uint8_t{0x20});
  const Result<InterruptEvents> e = driver.ServiceInterrupts();
  REQUIRE(e.ok());
  CHECK_EQ(e.value.status[0], uint8_t{0x20}); // reported from the latch
  CHECK_EQ(rig.Chip().Peek(kVbusLim), LimitByte(500));
}
} // namespace

TEST_CASE("R9 m-1 a charger byte read by ReadRegister still starts the input limit check") {
  CheckAfterRawRead(ClearMode::OnRead);
}

TEST_CASE("R9 m-1 the same on a chip that clears by a write") {
  CheckAfterRawRead(ClearMode::WriteOne);
}
