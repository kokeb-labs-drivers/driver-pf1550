/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "pf1550.hpp"
#include "pf1550_kconfig.hpp"

// --8<-- [start:custom]
pf1550::Config MyBoard() {
  pf1550::Config c{};
  // What the hardware tolerates: no defaults, Validate() rejects an unset limit.
  c.limits.cell_max_mv = 4200;
  c.limits.cell_max_charge_ma = 500;
  c.limits.input_max_ma = 1000;
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Sw1)] = {1000, 1200, 800}; // mV, mV, mA
  c.limits.rail[static_cast<std::size_t>(pf1550::Rail::Ldo1)] = {1700, 3300, 100};

  // The charger: voltage, current and input limit have no default.
  c.charger.apply = true;
  c.charger.mode = pf1550::ChargerMode::Charge;
  c.charger.charge_voltage_mv = 4200;
  c.charger.charge_current_ma = 300;
  c.charger.input_limit_ma = 500;

  // SW1 runs from the OTP value of your option; you declare that its voltage is a DVS table.
  c.sw1 = {true, pf1550::Dvs::Enabled,      pf1550::FromMillivolts(1100), {},
           {},   {true, true, false, false}};
  c.ldo1 = {true, pf1550::FromMillivolts(1800), {true, true, false, false}};
  return c;
}
// --8<-- [end:custom]

// --8<-- [start:kconfig]
pf1550::Config FromMenuconfig() {
  pf1550::Config c = pf1550::KconfigDefaults(); // the cell and supply limits, blocks unapplied
  c.charger.apply = true;
  c.charger.charge_voltage_mv = c.limits.cell_max_mv;
  c.charger.charge_current_ma = 300;
  c.charger.input_limit_ma = 500;
  return c;
}
// --8<-- [end:kconfig]

// --8<-- [start:validate]
static_assert(pf1550::Validate(pf1550::Config{}) == pf1550::Error::InvalidArgument,
              "no Limits: rejected");
static_assert(pf1550::Validate(pf1550::Config{.limits = pf1550::SingleCellLimits()}) ==
                  pf1550::Error::Ok,
              "a 4.2 V cell, 500 mA, 1 A supply");
// --8<-- [end:validate]
