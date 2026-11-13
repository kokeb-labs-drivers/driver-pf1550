/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include "mock_pf1550_bus.hpp"
#include "pf1550.hpp"

class VoidGpioPort : public pf1550_test::MockPortBase<VoidGpioPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
  void GpioSet(pf1550::CtrlPin pin, pf1550::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
  }
};

int main() {
  VoidGpioPort port;
  pf1550::PF1550<VoidGpioPort> sensor(port, pf1550::Config{});
  return static_cast<int>(sensor.EnsureInitialized().error);
}
