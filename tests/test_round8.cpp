/**
 * @file test_round8.cpp
 * @brief The eighth round (a fuzz finding): the reset evidence of a rail control register is the
 * bits the driver changed and verified; a bit it left alone is not a sentinel.
 */
#include <cstdint>

#include "test_support.hpp"

using namespace pf1550;
using namespace pf1550_test;

namespace {
const uint8_t kLdo1Ctrl = detail::RegsOf(Rail::Ldo1).ctrl;
constexpr uint8_t kStby = 0x02;
constexpr uint8_t kOmode = 0x04;
} // namespace

TEST_CASE("R8 a rail mode bit the driver left alone is no evidence of a reset") {
  Rig rig;
  Driver d(rig.port, rig.cfg);
  REQUIRE(d.EnsureInitialized().ok());
  rig.Chip().Poke(kLdo1Ctrl, 0x05); // run on, enabled in sleep (OMODE), standby off
  REQUIRE(d.SetRailModes(Rail::Ldo1, {true, false, false, false}).ok());
  CHECK_EQ(static_cast<uint32_t>(rig.Chip().Peek(kLdo1Ctrl)) & kOmode,
           0x00U); // only the sleep (OMODE) bit was changed
  // The standby bit reads differently now, but the driver never wrote it: not a reset.
  rig.Chip().Poke(kLdo1Ctrl, static_cast<uint8_t>(rig.Chip().Peek(kLdo1Ctrl) | kStby));
  CHECK(d.SelfCheck().error != Error::DeviceReset);
  // The bit the driver did change reads as before the write (OMODE set again): that is a reset.
  rig.Chip().Poke(kLdo1Ctrl, static_cast<uint8_t>(rig.Chip().Peek(kLdo1Ctrl) | kOmode));
  CHECK_EQ(d.SelfCheck().error, Error::DeviceReset);
}
