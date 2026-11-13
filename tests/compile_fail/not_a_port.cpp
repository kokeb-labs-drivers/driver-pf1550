/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "pf1550.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  pf1550::PF1550<NotAPort> sensor(port, pf1550::Config{});
  return static_cast<int>(sensor.EnsureInitialized().error);
}
