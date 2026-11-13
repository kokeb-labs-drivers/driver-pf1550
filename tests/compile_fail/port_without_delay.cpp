/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: the DVS-done wait needs the port's DelayUs().
 *
 * EXPECT: DelayUs
 */
#include "mock_pf1550_bus.hpp"
#include "pf1550.hpp"

class NoDelayPort : public pf1550_test::MockPortBase<NoDelayPort> {};

int main() {
  NoDelayPort port;
  pf1550::PF1550<NoDelayPort> sensor(port, pf1550::Config{});
  return static_cast<int>(
      sensor.SetRailVoltage(pf1550::Rail::Sw1, pf1550::FromMillivolts(1000)).error);
}
