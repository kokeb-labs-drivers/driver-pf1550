/**
 * @file test_support.hpp
 * @brief Shared fixtures of the host tests: a rig (port and chip) and a valid configuration.
 */
#ifndef PF1550_TEST_SUPPORT_HPP
#define PF1550_TEST_SUPPORT_HPP

#include "mock_pf1550_bus.hpp"
#include "pf1550.hpp"
#include "test_harness.hpp"

namespace pf1550_test {

using Port = FullPort;
using Driver = pf1550::PF1550<Port>;

/// A configuration that validates: a 4.2 V cell charged at up to 500 mA from a 1 A supply, rail
/// windows for SW1 (DVS), SW2, LDO1 and LDO3, and every block off (`apply` false).
inline pf1550::Config BaseConfig() {
  pf1550::Config c{};
  c.limits = pf1550::SingleCellLimits();
  c.limits.coin_cell_max_mv = 3300;
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Sw1)] = {1000, 1200, 800};
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Sw2)] = {1100, 1500, 800};
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Sw3)] = {1800, 3300, 500};
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Ldo1)] = {1700, 3300, 100};
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Ldo2)] = {1800, 3300, 100};
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Ldo3)] = {1700, 3300, 100};
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::VrefDdr)] = {600, 900, 5};
  return c;
}

/// A configuration that also writes the charger: 4.2 V, 500 mA, 1 A input, 4 h timer, thermistor
/// suspend, linear regulator only (charging is a separate request).
inline pf1550::Config ChargerConfigOn() {
  pf1550::Config c = BaseConfig();
  c.charger.apply = true;
  c.charger.mode = pf1550::ChargerMode::LinearOnly;
  c.charger.charge_voltage_mv = 4200;
  c.charger.charge_current_ma = 500;
  c.charger.input_limit_ma = 1000;
  return c;
}

/// The chip, its port and an initialised driver on option `option` (default A2: charger off).
struct Rig {
  Port port;
  pf1550::Config cfg;
  explicit Rig(uint8_t option = 2, const pf1550::Config& config = BaseConfig()) : cfg(config) {
    port.state.chip.Reset(option);
  }
  Pf1550Model& Chip() {
    return port.state.chip;
  }
};

/// Number of writes the chip saw.
inline std::size_t WriteCount(Rig& r) {
  return r.Chip().writes.size();
}

/// Whether the last write to `address` carried `value`; false when there was none.
inline bool LastWriteWas(Rig& r, uint8_t address, uint8_t value) {
  for (std::size_t i = r.Chip().writes.size(); i > 0; --i) {
    if (r.Chip().writes[i - 1].address == address) {
      return r.Chip().writes[i - 1].value == value;
    }
  }
  return false;
}

/// Whether any write went to `address`.
inline bool WroteTo(Rig& r, uint8_t address) {
  bool found = false;
  for (const WriteRecord& w : r.Chip().writes) {
    found = found || (w.address == address);
  }
  return found;
}

} // namespace pf1550_test

#endif // PF1550_TEST_SUPPORT_HPP
