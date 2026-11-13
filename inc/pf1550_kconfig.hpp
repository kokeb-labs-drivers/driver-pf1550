/**
 * @file pf1550_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_PF1550_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to the datasheet default, so
 * `KconfigDefaults()` equals `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so an absent
 * `CONFIG_PF1550_CRC_ENABLE` means off in both cases.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h`
 * per build; each host test is its own program). No driver or library header includes this one
 * (handbook KB-CPP20-018).
 */
#ifndef PF1550_KCONFIG_HPP
#define PF1550_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "pf1550_config.hpp"

#ifndef CONFIG_PF1550_CELL_MAX_MV
/** Highest cell charge voltage in mV when Kconfig does not set it: 4200, a standard Li-ion cell. */
#define CONFIG_PF1550_CELL_MAX_MV 4200
#endif
#ifndef CONFIG_PF1550_CELL_MAX_CHARGE_MA
/** Highest cell charge current in mA when Kconfig does not set it: 500. */
#define CONFIG_PF1550_CELL_MAX_CHARGE_MA 500
#endif
#ifndef CONFIG_PF1550_INPUT_MAX_MA
/** Supply current in mA when Kconfig does not set it: 1000. */
#define CONFIG_PF1550_INPUT_MAX_MA 1000
#endif

namespace pf1550 {

/**
 * @brief The configuration menuconfig selected: the cell and supply `Limits` and the as-found
 * policy, with every block left unapplied.
 * @return A `Config` that `Validate()` accepts; rail windows are not set, so no rail is
 * configurable until the application sets them.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
  c.limits.cell_max_mv = CONFIG_PF1550_CELL_MAX_MV;
  c.limits.cell_max_charge_ma = CONFIG_PF1550_CELL_MAX_CHARGE_MA;
  c.limits.input_max_ma = CONFIG_PF1550_INPUT_MAX_MA;
#if defined(CONFIG_PF1550_AS_FOUND_MAKE_SAFE) && CONFIG_PF1550_AS_FOUND_MAKE_SAFE
  c.as_found_policy = AsFoundPolicy::MakeSafe;
#endif
  return c;
}

} // namespace pf1550

#endif // PF1550_KCONFIG_HPP
