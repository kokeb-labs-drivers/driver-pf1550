/**
 * @file config_without_limits.cpp
 * @brief Must NOT compile: a configuration that has no `Limits` fails `Validate()` at compile time
 * when the application asserts it (handbook KB-CPP20-013).
 *
 * EXPECT: static assertion failed
 */
#include "pf1550_config.hpp"

int main() {
  constexpr pf1550::Config kBoard{}; // the Limits have no defaults
  static_assert(pf1550::Validate(kBoard) == pf1550::Error::Ok, "limits are mandatory");
  return 0;
}
