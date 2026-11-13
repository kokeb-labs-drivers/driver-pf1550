/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "pf1550.hpp"

int start_pmic(MyPort& port) {
  pf1550::Config config{};
  config.limits.cell_max_mv = 4200;          // from your cell's data sheet
  config.limits.cell_max_charge_ma = 500;    // from your cell's data sheet
  config.limits.input_max_ma = 1000;         // what your supply delivers
  pf1550::PF1550<MyPort> pmic(port, config); // no I/O yet

  if (auto r = pmic.EnsureInitialized(); !r) { // 1. bus up, DEVICE_ID, charger as found
    return static_cast<int>(r.error); //    NoDevice, DeviceIdMismatch, Unsafe, OutOfLimits ...
  }
  const auto status = pmic.ReadChargerStatus(); // 2. read only
  if (!status) {
    return static_cast<int>(status.error);
  }
  return static_cast<int>(status.value.fault); // 3. 0 is Fault::None
}
// --8<-- [end:quickstart]
