/**
 * @file host_pf1550_port.hpp
 * @brief The host port: the driver's bus hooks over a simulated PF1550, so the example runs on any
 * PC.
 *
 * `pf1550_test::MockPortBase` (tests/mock_pf1550_bus.hpp) supplies the required bus hooks over
 * the register model the host tests use. This class adds what a real port adds: `DelayUs()` (here
 * it advances the simulated chip's clock instead of waiting) and `Log()` to the console. A port for
 * a real bus on a PC (Linux spidev, a USB bridge) implements the same hooks: see docs/porting.md.
 */
#ifndef PF1550_HOST_PF1550_PORT_HPP
#define PF1550_HOST_PF1550_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "mock_pf1550_bus.hpp"

/// The host port: simulated chip, real console.
class HostPf1550Port : public pf1550_test::MockPortBase<HostPf1550Port> {
public:
  /// The bus settings a real port would use; checked against the driver's bus requirements
  /// (handbook KB-CPP-123) like any port's.
  static constexpr uint32_t kClockHz = 400'000;
  static_assert(kClockHz <= pf1550::kMaxClockHz, "SCL above the PF1550's maximum");

  /// Time passes in the simulated chip; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    state.chip.Elapse(us);
  }

  /// Driver messages (they carry their own `pf1550:` prefix) go to the console, one line each.
  void Log(pf1550::LogLevel level, const char* message) noexcept {
    (void)level;
    (void)std::puts(message); // plain text: never pass it as a format string
  }
};

#endif // PF1550_HOST_PF1550_PORT_HPP
