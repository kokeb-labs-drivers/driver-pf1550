/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstdint>

#include "mock_pf1550_bus.hpp"
#include "pf1550.hpp"

class ShortDelayPort : public pf1550_test::MockPortBase<ShortDelayPort> {
public:
  void DelayUs(uint16_t us) noexcept {
    (void)us;
  }
};

int main() {
  ShortDelayPort port;
  pf1550::PF1550<ShortDelayPort> sensor(port, pf1550::Config{});
  return static_cast<int>(sensor.EnsureInitialized().error);
}
