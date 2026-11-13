/**
 * @file test_round9.cpp
 * @brief The eighth review round: a charger interrupt consumed by a failed collect phase is still
 * checked by the next successful service (R8-1: each transfer of the call is failed in turn, the
 * count taken from a clean run, with and without `ClearLatchedEvents()`), and the guards the
 * review's mutants showed untested: a failed mask write leaves no idle shortcut armed, a failed
 * service leaves its bits in the latch, a failed switch-off is a bus error, `ClearLatchedEvents()`
 * empties the latch, and a bus error of the VBUS_SNS read is not an Ok.
 */
#include <algorithm>
#include <cstdint>

#include "pf1550_irq.hpp"
#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
constexpr uint8_t kChgOper = 0x89;
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

struct Live {
  Rig rig;
  Driver driver;
  explicit Live(const Config& c = Limit500Config()) : rig(2, c), driver(rig.port, rig.cfg) {
    CHECK(driver.EnsureInitialized().ok());
  }
  void Charging() {
    REQUIRE(driver.Configure().ok());
    REQUIRE(driver.SetChargerMode(ChargerMode::Charge).ok());
    rig.Chip().env.vbus_valid = true;
  }
  void Unmask(ClearMode mode) {
    rig.Chip().readings.int_clear = mode;
    REQUIRE(driver.SetIrqMask(IrqMask{{0, 0, 0, 0, 0, 0, 0, 0}}).ok());
  }
  /// A cable removed (the limit goes back to the OTP value) and the charger interrupt raised.
  void CableRemovedWithInterrupt() {
    rig.Chip().VbusRemoved();
    rig.Chip().Raise(0x80, 5);
  }
};

/// The transfers one call makes when nothing fails, set up as the sweep sets it up. A sweep fails
/// each transfer index from 0 to this count: the last index lies past the call, so the sweep ends
/// with a call that succeeds, and every transfer of the call has been failed once.
template <typename Setup, typename Call>
int CleanTransfers(Setup setup, Call call) {
  Live l(setup.config);
  setup(l);
  const int before = l.rig.port.state.transfers;
  call(l);
  return l.rig.port.state.transfers - before;
}

struct ChargingSetup {
  Config config = Limit500Config();
  ClearMode mode = ClearMode::OnRead;
  void operator()(Live& l) const {
    l.Charging();
    l.CableRemovedWithInterrupt();
    l.Unmask(mode);
  }
};

bool AnyLatched(const InterruptEvents& e) {
  return std::any_of(e.status.begin(), e.status.end(), [](uint8_t s) { return s != 0U; });
}

void SweepFailedService(ClearMode mode, bool clear_latch) {
  ChargingSetup setup;
  setup.mode = mode;
  const int clean =
      CleanTransfers(setup, [](Live& l) { CHECK(l.driver.ServiceInterrupts().ok()); });
  CHECK(clean > 3); // the scenario reaches the check (a second pass, the restore and its read-back)
  int failed = 0;
  for (int k = 0; k <= clean; ++k) {
    Live l;
    setup(l);
    l.rig.port.state.fail_transfer = l.rig.port.state.transfers + k;
    const bool first = l.driver.ServiceInterrupts().ok();
    l.rig.port.state.fail_transfer = -1;
    CHECK_EQ(first, k == clean); // every transfer of the call matters; the last index is past it
    failed += first ? 0 : 1;
    if (clear_latch) {
      l.driver.ClearLatchedEvents();
    }
    CHECK(l.driver.ServiceInterrupts().ok());
    CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500));
  }
  CHECK_EQ(failed, clean);
}
} // namespace

// ---- R8-1 --------------------------------------------------------------------------------------

TEST_CASE("R8-1 a failed service never loses the check of a consumed charger interrupt") {
  SweepFailedService(ClearMode::OnRead, false);
}

TEST_CASE("R8-1 the same with the latch cleared between the calls") {
  SweepFailedService(ClearMode::OnRead, true);
}

TEST_CASE("R8-1 the same when the status is cleared by a write") {
  SweepFailedService(ClearMode::WriteOne, false);
  SweepFailedService(ClearMode::WriteOne, true);
}

// ---- M20: a failed mask write -------------------------------------------------------------------

TEST_CASE("R8 a failed mask write leaves no idle shortcut armed") {
  // A failure before any mask register is touched (the liveness reads) leaves the shortcut as it
  // was; a failure of a mask write may leave a partly written mask, so the chip is asked again.
  bool asked_again = false;
  for (int k = 0; k <= 6; ++k) {
    Live l;
    l.Unmask(ClearMode::OnRead); // all sources visible: INTB high proves an idle chip
    l.rig.port.state.fail_transfer = l.rig.port.state.transfers + k;
    const bool set = l.driver.SetIrqMask(IrqMask{{0xFF, 0xFF, 0, 0, 0, 0, 0, 0}}).ok();
    l.rig.port.state.fail_transfer = -1;
    if (set) {
      continue;
    }
    const int before = l.rig.port.state.transfers;
    CHECK(l.driver.ServiceInterrupts().ok());
    asked_again = asked_again || (l.rig.port.state.transfers > before);
  }
  CHECK(asked_again);
}

// ---- M21, M33, M30: the latch --------------------------------------------------------------

TEST_CASE("R8 after a failed service the bits stay in the latch, and clearing it empties it") {
  Live l;
  l.Charging();
  l.CableRemovedWithInterrupt();
  l.Unmask(ClearMode::OnRead);
  l.rig.port.state.fail_transfer = l.rig.port.state.transfers + 3; // the second pass
  CHECK_EQ(l.driver.ServiceInterrupts().error, Error::Bus);
  l.rig.port.state.fail_transfer = -1;
  CHECK(AnyLatched(l.driver.GetLatchedEvents()));
  l.driver.ClearLatchedEvents();
  CHECK(!AnyLatched(l.driver.GetLatchedEvents()));
  CHECK(!l.driver.GetLatchedEvents().fault_class);
}

// ---- M26: a switch-off that fails ---------------------------------------------------------

TEST_CASE("R8 a switch-off that fails is a bus error, not an out of limits answer") {
  struct AboveLimits {
    Config config = BaseConfig(); // no configured limit: an input above Limits stops the charger
    void operator()(Live& l) const {
      l.rig.Chip().Poke(kChgOper, 0x02);
      l.rig.Chip().Poke(kVbusLim, 0xA0); // 1500 mA, above Limits
    }
  };
  const AboveLimits setup;
  const int clean = CleanTransfers(
      setup, [](Live& l) { CHECK_EQ(l.driver.ReadChargerStatus().error, Error::OutOfLimits); });
  bool bus = false;
  for (int k = 0; k <= clean; ++k) {
    Live l(setup.config);
    setup(l);
    l.rig.port.state.fail_transfer = l.rig.port.state.transfers + k;
    const Result<ChargerStatus> r = l.driver.ReadChargerStatus();
    l.rig.port.state.fail_transfer = -1;
    if (r.error == Error::OutOfLimits) {
      CHECK_EQ(l.rig.Chip().Peek(kChgOper) & 0x03U, 0x00U); // it says so only once it is off
    }
    bus = bus || (r.error == Error::Bus);
  }
  CHECK(bus);
}

// ---- M35: the supply read of a restore -----------------------------------------------------

TEST_CASE("R8 a restore whose supply read fails is an error, never an Ok without the restore") {
  struct Removed {
    Config config = Limit500Config();
    void operator()(Live& l) const {
      l.Charging();
      l.rig.Chip().VbusRemoved();
    }
  };
  const Removed setup;
  const int clean =
      CleanTransfers(setup, [](Live& l) { CHECK(l.driver.ReadChargerStatus().ok()); });
  bool failed_once = false;
  for (int k = 0; k <= clean; ++k) {
    Live l(setup.config);
    setup(l);
    l.rig.port.state.fail_transfer = l.rig.port.state.transfers + k;
    const bool ok = l.driver.ReadChargerStatus().ok();
    l.rig.port.state.fail_transfer = -1;
    if (ok) {
      CHECK_EQ(l.rig.Chip().Peek(kVbusLim), LimitByte(500)); // an Ok means the limit is held
    } else {
      failed_once = true;
    }
  }
  CHECK(failed_once);
}
