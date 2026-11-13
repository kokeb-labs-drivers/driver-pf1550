/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "pf1550_kconfig.hpp"
#include "test_harness.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Limits and the policy") {
  constexpr pf1550::Config kC = pf1550::KconfigDefaults();
  CHECK_EQ(kC.limits.cell_max_mv, 4100U);
  CHECK_EQ(kC.limits.cell_max_charge_ma, 300U);
  CHECK_EQ(kC.limits.input_max_ma, 500U);
  CHECK(kC.as_found_policy == pf1550::AsFoundPolicy::MakeSafe);
  CHECK_EQ(pf1550::Validate(kC), pf1550::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() is a valid single-cell configuration") {
  constexpr pf1550::Config kC = pf1550::KconfigDefaults();
  CHECK_EQ(kC.limits.cell_max_mv, 4200U);
  CHECK_EQ(kC.limits.cell_max_charge_ma, 500U);
  CHECK_EQ(kC.limits.input_max_ma, 1000U);
  CHECK(kC.as_found_policy == pf1550::AsFoundPolicy::Report);
  CHECK_EQ(pf1550::Validate(kC), pf1550::Error::Ok);
}
#endif
