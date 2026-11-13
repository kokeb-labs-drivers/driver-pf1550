/**
 * @file main.cpp
 * @brief The PF1550 driver on your PC: start the driver against a simulated chip, configure the
 * charger and one rail, read the charger status and service the interrupts.
 *
 * No hardware and no SDK: the host port (host_pf1550_port.hpp) connects the real driver to the
 * register model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). Exit code 0 when every call succeeded.
 */
#include <cstdint>
#include <cstdio>

#include "host_pf1550_port.hpp"
#include "pf1550.hpp"

namespace {

/// What this application's hardware tolerates: the numbers come from the cell's and the supply's
/// data sheets, not from the PF1550's.
pf1550::Config MyConfig() {
  pf1550::Config c{};
  c.limits.cell_max_mv = 4200;
  c.limits.cell_max_charge_ma = 500;
  c.limits.input_max_ma = 1000;
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Ldo1)] = {
      .min_mv = 1700, .max_mv = 3300, .max_load_ma = 100};
  c.charger.apply = true;
  c.charger.mode = pf1550::ChargerMode::Charge;
  c.charger.charge_voltage_mv = 4200;
  c.charger.charge_current_ma = 300;
  c.charger.input_limit_ma = 500;
  c.ldo1 = {.apply = true,
            .volt = pf1550::FromMillivolts(1800),
            .modes = {.run = true, .standby = true, .sleep = false, .low_power = false}};
  return c;
}

int Fail(const char* what, pf1550::Error error) {
  std::printf("%s failed: %s\n", what, pf1550::ToString(error));
  return 1;
}

} // namespace

int main() {
  HostPf1550Port port;
  port.state.chip.env.vbus_valid = true;                 // a charger is plugged in
  pf1550::PF1550<HostPf1550Port> pmic(port, MyConfig()); // no I/O yet

  std::printf("PF1550 driver %s on the host\n", pf1550::GetDriverVersion());
  if (const auto r = pmic.EnsureInitialized(); !r) {
    return Fail("EnsureInitialized", r.error);
  }
  if (const auto r = pmic.Configure(); !r) {
    return Fail("Configure", r.error);
  }
  const auto status = pmic.ReadChargerStatus();
  if (!status) {
    return Fail("ReadChargerStatus", status.error);
  }
  std::printf("charger state %d, fault %d, VBUS valid %d\n", static_cast<int>(status.value.state),
              static_cast<int>(status.value.fault), static_cast<int>(status.value.vbus.valid));
  const auto ldo = pmic.ReadRail(pf1550::Rail::Ldo1);
  if (!ldo) {
    return Fail("ReadRail", ldo.error);
  }
  std::printf("LDO1 %u uV, run %d\n", static_cast<unsigned>(ldo.value.volt.value),
              static_cast<int>(ldo.value.enabled_run));
  // The chip resets in the middle of a run (a power dip, a watchdog): the next call that changes
  // the chip says `DeviceReset` and writes nothing; start the driver again and write the
  // configuration.
  port.state.chip.Reset();
  const auto watchdog = pmic.ServiceChargerWatchdog();
  if (watchdog.error == pf1550::Error::DeviceReset) {
    std::printf("the chip reset, starting again\n");
    if (const auto r = pmic.EnsureInitialized(); !r) {
      return Fail("EnsureInitialized after a reset", r.error);
    }
    if (const auto r = pmic.Configure(); !r) {
      return Fail("Configure after a reset", r.error);
    }
  }
  const auto events = pmic.ServiceInterrupts();
  if (!events) {
    return Fail("ServiceInterrupts", events.error);
  }
  std::printf("%u interrupt passes\n", static_cast<unsigned>(events.value.passes));
  return 0;
}
