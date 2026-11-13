/**
 * @file pf1550_interrupt_example.cpp
 * @brief Charge with the charger watchdog on, and react to INTB: log each event, service the
 * watchdog every 10 s and restart the charger after a fault.
 *
 * Wiring: examples/esp32/README.md (INTB and STANDBY pins). Every Result is checked and logged.
 */
#include <array>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_pf1550_port.hpp"
#include "pf1550.hpp"
#include "pf1550_kconfig.hpp"

namespace {
constexpr const char* kTag = "pf1550_irq";
constexpr int kWatchdogPeriodMs = 10'000; // at most pf1550::kWatchdogServiceMaxS seconds

pf1550::Config MyConfig() {
  pf1550::Config c = pf1550::KconfigDefaults();
  c.charger.apply = true;
  c.charger.mode = pf1550::ChargerMode::LinearOnly;
  c.charger.charge_voltage_mv = c.limits.cell_max_mv;
  c.charger.charge_current_ma = 300;
  c.charger.input_limit_ma = 500;
  c.charger.watchdog.enable = true;
  c.apply_irq_mask = true;
  c.irq_mask = pf1550::UnmaskOnly(std::array<pf1550::IrqSource, 3>{
      pf1550::IrqSource::ChgVbus, pf1550::IrqSource::ChgCharger, pf1550::IrqSource::ChgThermistor});
  return c;
}

/// The PMIC reset in the middle of the run (UVDET, a watchdog, a REGS_DISABLE pulse): the driver
/// says `DeviceReset` and is uninitialised. Start it again, write the configuration, charge again.
bool Recover(pf1550::PF1550<Esp32Pf1550Port>& pmic) {
  ESP_LOGW(kTag, "the PMIC reset, starting again");
  return pmic.EnsureInitialized().ok() && pmic.Configure().ok() &&
         pmic.SetChargerMode(pf1550::ChargerMode::Charge).ok();
}
} // namespace

extern "C" void app_main(void) {
  static Esp32Pf1550Port port;
  static pf1550::PF1550<Esp32Pf1550Port> pmic(port, MyConfig());

  if (const auto r = pmic.EnsureInitialized(); !r) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", pf1550::ToString(r.error));
    return;
  }
  if (const auto r = pmic.Configure(); !r) {
    ESP_LOGE(kTag, "Configure failed: %s", pf1550::ToString(r.error));
    return;
  }
  if (const auto r = pmic.SetChargerMode(pf1550::ChargerMode::Charge); !r) {
    ESP_LOGW(kTag, "SetChargerMode failed: %s", pf1550::ToString(r.error));
  }

  int since_watchdog_ms = 0;
  while (true) {
    const auto events = pmic.ServiceInterrupts(); // no bus traffic while INTB is high
    if (events && pf1550::Has(events.value, pf1550::IrqSource::ChgCharger)) {
      const auto status = pmic.ReadChargerStatus();
      if (status && (status.value.fault != pf1550::Fault::None)) {
        ESP_LOGW(kTag, "charger fault %d, restarting", static_cast<int>(status.value.fault));
        if (const auto r = pmic.RestartCharger(); !r) {
          ESP_LOGE(kTag, "RestartCharger failed: %s", pf1550::ToString(r.error));
        }
      }
    } else if (!events) {
      ESP_LOGW(kTag, "ServiceInterrupts failed: %s", pf1550::ToString(events.error));
    } else {
      // No charger event.
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    since_watchdog_ms += 100;
    if (since_watchdog_ms >= kWatchdogPeriodMs) {
      since_watchdog_ms = 0;
      if (const auto r = pmic.ServiceChargerWatchdog(); !r) {
        ESP_LOGE(kTag, "ServiceChargerWatchdog failed: %s", pf1550::ToString(r.error));
        if ((r.error == pf1550::Error::DeviceReset) || (r.error == pf1550::Error::NotInitialized)) {
          static_cast<void>(Recover(pmic)); // logged inside; the next period tries again
        }
      }
    }
  }
}
