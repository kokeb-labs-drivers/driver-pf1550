/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include <cstdio>

#include "my_port.hpp"
#include "pf1550.hpp"

using Pmic = pf1550::PF1550<MyPort>;

// --8<-- [start:lifecycle]
pf1550::Error Start(Pmic& pmic) {
  if (auto r = pmic.EnsureInitialized(); !r) { // validates, identifies, checks the charger as found
    return r.error;
  }
  return pmic.Configure().error; // writes the blocks whose `apply` is true, charger last
}
// --8<-- [end:lifecycle]

// --8<-- [start:charger]
pf1550::Error WatchCharger(Pmic& pmic) {
  const auto s = pmic.ReadChargerStatus();
  if (!s) {
    return s.error;
  }
  if (s.value.fault == pf1550::Fault::TimerFault) {
    return pmic.RestartCharger().error; // off, then on; at most three tries in a row
  }
  return pmic.ServiceChargerWatchdog().error; // every 16 s or less when the watchdog is on
}
// --8<-- [end:charger]

// --8<-- [start:rails]
pf1550::Error Rails(Pmic& pmic) {
  if (auto r = pmic.SetRailVoltage(pf1550::Rail::Ldo1, pf1550::FromMillivolts(2800)); !r) {
    return r.error; // OutOfLimits when your Limits window does not allow it
  }
  return pmic.SetRailModes(pf1550::Rail::Ldo1, {true, true, false, false}).error;
}
// --8<-- [end:rails]

// --8<-- [start:interrupts]
pf1550::Error OnIntb(Pmic& pmic) {
  const auto e = pmic.ServiceInterrupts(); // reads, copies out, clears
  if (!e) {
    return e.error;
  }
  if (pf1550::Has(e.value, pf1550::IrqSource::Onkey3s)) {
    std::printf("ONKEY held for 3 s\n");
  }
  return e.value.fault_class ? pf1550::Error::DeviceFault : pf1550::Error::Ok;
}
// --8<-- [end:interrupts]

// --8<-- [start:errors]
const char* Explain(pf1550::Error e) {
  switch (e) {
  case pf1550::Error::OutOfLimits:
    return "the value is valid for the chip but outside your Limits";
  case pf1550::Error::Unsafe:
    return "a safety downgrade needs its Acknowledgements flag";
  case pf1550::Error::Refused:
    return "a precondition of a dangerous call is not met";
  case pf1550::Error::NoDevice:
    return "nothing acknowledged the address";
  default:
    return pf1550::ToString(e);
  }
}
// --8<-- [end:errors]

// --8<-- [start:ship]
pf1550::Error ShipMode(Pmic& pmic) {
  // Cuts every supply, VSNVS included; recovery is a charger or a battery attach.
  return pmic.EnterShip(pf1550::ShipConfirm{}).error;
}
// --8<-- [end:ship]

// --8<-- [start:sequencer]
void Sequencer() {
  // Table 85: the start-up order of option A4 and when each rail turns on (microseconds).
  const auto profile = pf1550::StartupProfileOf(pf1550::Option::A4);
  if (profile) {
    const uint32_t ldo1_us = pf1550::PowerUpDelayUs(profile.value, pf1550::Rail::Ldo1);
    const uint32_t total_us = pf1550::PowerUpDurationUs(profile.value);
    std::printf("LDO1 at %u us of %u us\n", static_cast<unsigned>(ldo1_us),
                static_cast<unsigned>(total_us));
  }
}
// --8<-- [end:sequencer]
