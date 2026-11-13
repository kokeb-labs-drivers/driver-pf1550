/**
 * @file pf1550_basic_example.cpp
 * @brief Start the PF1550, write the application's configuration, then log the charger and LDO1
 * once a second.
 *
 * Wiring: examples/esp32/README.md. Expected output: one "initialised" line with the variant, one
 * "configured" line, then a status line per second. Every Result is checked and logged by name.
 * The limits come from menuconfig (pf1550::KconfigDefaults()); the rest of the configuration is
 * what this application wants the chip to do.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_pf1550_port.hpp"
#include "pf1550.hpp"
#include "pf1550_kconfig.hpp"

namespace {
constexpr const char* kTag = "pf1550_example";

pf1550::Config MyConfig() {
  pf1550::Config c = pf1550::KconfigDefaults();
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Ldo1)] = {1700, 3300, 100};
  c.charger.apply = true;
  c.charger.mode = pf1550::ChargerMode::LinearOnly; // charging is a separate request
  c.charger.charge_voltage_mv = c.limits.cell_max_mv;
  c.charger.charge_current_ma = 300;
  c.charger.input_limit_ma = 500;
  c.ldo1 = {true, pf1550::FromMillivolts(1800), {true, true, false, false}};
  return c;
}
} // namespace

extern "C" void app_main(void) {
  static Esp32Pf1550Port port; // static: the driver keeps a reference
  static pf1550::PF1550<Esp32Pf1550Port> pmic(port, MyConfig()); // no I/O

  ESP_LOGI(kTag, "PF1550 driver %s", pf1550::GetDriverVersion());
  if (const auto r = pmic.EnsureInitialized(); !r) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", pf1550::ToString(r.error));
    return;
  }
  const auto variant = pmic.ReadVariant();
  if (variant) {
    ESP_LOGI(kTag, "initialised, option %d", static_cast<int>(variant.value.option));
  }
  if (const auto r = pmic.Configure(); !r) {
    ESP_LOGE(kTag, "Configure failed: %s", pf1550::ToString(r.error));
    return;
  }
  ESP_LOGI(kTag, "configured");
  if (const auto r = pmic.SetChargerMode(pf1550::ChargerMode::Charge); !r) {
    ESP_LOGW(kTag, "SetChargerMode failed: %s", pf1550::ToString(r.error));
  }

  while (true) {
    const auto status = pmic.ReadChargerStatus();
    const auto ldo = pmic.ReadRail(pf1550::Rail::Ldo1);
    if (status && ldo) {
      ESP_LOGI(kTag, "charger state %d, fault %d, LDO1 %u uV", static_cast<int>(status.value.state),
               static_cast<int>(status.value.fault), static_cast<unsigned>(ldo.value.volt.value));
    } else {
      ESP_LOGW(kTag, "read failed: %s", pf1550::ToString(status ? ldo.error : status.error));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
