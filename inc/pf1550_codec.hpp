/**
 * @file pf1550_codec.hpp
 * @brief Register codecs of the PF1550: voltage and current grids of the data sheet tables, as
 * `constexpr` functions that are total only over the documented codes.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Every encoder returns `InvalidArgument` for a value that is off the grid or in a code the data
 * sheet labels Reserved; every decoder returns `InvalidArgument` for a reserved code. The anchors
 * of each table are proved with `static_assert` at the end of the file (KB-CPP20-012).
 */
#ifndef PF1550_CODEC_HPP
#define PF1550_CODEC_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_detail.hpp"
#include "pf1550_types.hpp"

namespace pf1550 {

/**
 * @defgroup pf1550_codec Register codecs
 * @brief Voltage and current grids (datasheet Tables 31, 37, 41, 43, 111, 158 to 160, 163).
 * @{
 */

/// Lowest charge voltage (CHGCV code 8), mV (Table 160).
inline constexpr uint32_t kChargeVoltageMinMv = 3500;
/// Highest charge voltage (CHGCV code 55), mV.
inline constexpr uint32_t kChargeVoltageMaxMv = 4440;
/// Charge current range, mA (Table 159: codes 0 to 18).
inline constexpr uint32_t kChargeCurrentMinMa = 100;
/// Highest charge current, mA.
inline constexpr uint32_t kChargeCurrentMaxMa = 1000;
/// DVS buck lowest voltage, uV (Table 31).
inline constexpr uint32_t kBuckDvsMinUv = 600000;
/// DVS buck step, uV.
inline constexpr uint32_t kBuckDvsStepUv = 12500;
/// Number of VBUS_LIN_ILIM codes the data sheet defines (Table 163).
inline constexpr std::size_t kInputLimitCodes = 21;

/// The VBUS_LIN_ILIM table, mA, indexed by code (Table 163, Table 58).
inline constexpr std::array<uint16_t, kInputLimitCodes> kInputLimitMa = {
    10,  15,  20,  25,  30,  35,  40,  45,  50,   100, 150,
    200, 300, 400, 500, 600, 700, 800, 900, 1000, 1500};

/// The fixed (DVS disabled) buck table, uV, codes 0 to 6; every code above is 3.3 V (Table 31).
inline constexpr std::array<uint32_t, 7> kBuckFixedUv = {1100000, 1200000, 1350000, 1500000,
                                                         1800000, 2500000, 3000000};

/**
 * @brief CHGCV code for a charge voltage.
 * @param mv Millivolts, 3500 to 4440 in 20 mV steps.
 * @return The code 8 to 55, or `InvalidArgument`.
 */
constexpr Result<uint8_t> ChargeVoltageCode(uint32_t mv) noexcept {
  if ((mv < kChargeVoltageMinMv) || (mv > kChargeVoltageMaxMv) ||
      (((mv - kChargeVoltageMinMv) % 20U) != 0U)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint8_t>::Success(static_cast<uint8_t>(8U + ((mv - kChargeVoltageMinMv) / 20U)));
}

/**
 * @brief The charge voltage a CHGCV code stands for.
 * @param code 0 to 63: 0 to 8 are 3.50 V, 56 to 63 are 4.44 V.
 * @return Millivolts.
 */
constexpr uint32_t ChargeVoltageMv(uint32_t code) noexcept {
  if (code <= 8U) {
    return kChargeVoltageMinMv;
  }
  if (code >= 55U) {
    return kChargeVoltageMaxMv;
  }
  return kChargeVoltageMinMv + (20U * (code - 8U));
}

/**
 * @brief CHG_CC code for a charge current.
 * @param ma 100 to 1000 in 50 mA steps.
 * @return The code 0 to 18, or `InvalidArgument`.
 */
constexpr Result<uint8_t> ChargeCurrentCode(uint32_t ma) noexcept {
  if ((ma < kChargeCurrentMinMa) || (ma > kChargeCurrentMaxMa) || (((ma - 100U) % 50U) != 0U)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint8_t>::Success(static_cast<uint8_t>((ma - 100U) / 50U));
}

/**
 * @brief The current of a CHG_CC code.
 * @param code 0 to 18 (19 to 31 are Reserved).
 * @return mA, or `InvalidArgument` for a reserved code.
 */
constexpr Result<uint32_t> ChargeCurrentMa(uint32_t code) noexcept {
  if (code > 18U) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint32_t>::Success(100U + (50U * code));
}

/**
 * @brief IEOC code for a termination current.
 * @param ma 5, 10, 20, 30 or 50.
 * @return The code 0 to 4, or `InvalidArgument`.
 */
constexpr Result<uint8_t> TerminationCode(uint32_t ma) noexcept {
  constexpr std::array<uint32_t, 5> kTable = {5, 10, 20, 30, 50};
  for (std::size_t i = 0U; i < kTable.size(); ++i) {
    if (kTable[i] == ma) {
      return Result<uint8_t>::Success(static_cast<uint8_t>(i));
    }
  }
  return Result<uint8_t>::Failure(Error::InvalidArgument);
}

/**
 * @brief The termination current of an IEOC code.
 * @param code 0 to 4 (5 to 7 are reserved).
 * @return mA, or `InvalidArgument`.
 */
constexpr Result<uint32_t> TerminationMa(uint32_t code) noexcept {
  constexpr std::array<uint32_t, 5> kTable = {5, 10, 20, 30, 50};
  if (code >= kTable.size()) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint32_t>::Success(kTable[code]);
}

/**
 * @brief VBUS_LIN_ILIM code for an input current limit.
 * @param ma One of the 21 values of `kInputLimitMa`.
 * @return The code 0 to 20, or `InvalidArgument`.
 */
constexpr Result<uint8_t> InputLimitCode(uint32_t ma) noexcept {
  for (std::size_t i = 0U; i < kInputLimitMa.size(); ++i) {
    if (kInputLimitMa[i] == ma) {
      return Result<uint8_t>::Success(static_cast<uint8_t>(i));
    }
  }
  return Result<uint8_t>::Failure(Error::InvalidArgument);
}

/**
 * @brief The limit of a VBUS_LIN_ILIM code.
 * @param code 0 to 20 (21 to 31 are reserved).
 * @return mA, or `InvalidArgument`.
 */
constexpr Result<uint32_t> InputLimitMa(uint32_t code) noexcept {
  if (code >= kInputLimitMa.size()) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint32_t>::Success(kInputLimitMa[code]);
}

/**
 * @brief DVS-mode buck code for a voltage (SW1, SW2: 0.6 V + 12.5 mV x code).
 * @param uv 600000 to 1387500 uV on the 12.5 mV grid.
 * @return The code 0 to 63, or `InvalidArgument`.
 */
constexpr Result<uint8_t> BuckDvsCode(uint32_t uv) noexcept {
  if ((uv < kBuckDvsMinUv) || (uv > (kBuckDvsMinUv + (63U * kBuckDvsStepUv))) ||
      (((uv - kBuckDvsMinUv) % kBuckDvsStepUv) != 0U)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint8_t>::Success(static_cast<uint8_t>((uv - kBuckDvsMinUv) / kBuckDvsStepUv));
}

/**
 * @brief The voltage of a DVS-mode buck code.
 * @param code 0 to 63.
 * @return uV.
 */
constexpr uint32_t BuckDvsUv(uint32_t code) noexcept {
  return kBuckDvsMinUv + (kBuckDvsStepUv * (code & 0x3FU));
}

/**
 * @brief The voltage of a buck code in the non-DVS table (Table 31): codes 0 to 6, 3.3 V above.
 * @param code 0 to 63.
 * @return uV.
 */
constexpr uint32_t BuckFixedUvOf(uint32_t code) noexcept {
  return (code < kBuckFixedUv.size()) ? kBuckFixedUv[code] : 3300000U;
}

/**
 * @brief The voltage of an SW3 code (1.80 V + 100 mV x code, Table 37).
 * @param code 0 to 15.
 * @return uV.
 */
constexpr uint32_t Sw3Uv(uint32_t code) noexcept {
  return 1800000U + (100000U * (code & 0x0FU));
}

/**
 * @brief LDO voltage code.
 * @param rail `Rail::Ldo1`, `Rail::Ldo3` (5-bit table, Table 41) or `Rail::Ldo2` (4-bit, Table 43).
 * @param uv Microvolts on the table's grid.
 * @return The code, or `InvalidArgument`.
 */
constexpr Result<uint8_t> LdoCode(Rail rail, uint32_t uv) noexcept {
  if (rail == Rail::Ldo2) {
    if ((uv >= 1800000U) && (uv <= 3300000U) && (((uv - 1800000U) % 100000U) == 0U)) {
      return Result<uint8_t>::Success(static_cast<uint8_t>((uv - 1800000U) / 100000U));
    }
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  if ((rail != Rail::Ldo1) && (rail != Rail::Ldo3)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  if ((uv >= 750000U) && (uv <= 1500000U) && (((uv - 750000U) % 50000U) == 0U)) {
    return Result<uint8_t>::Success(static_cast<uint8_t>((uv - 750000U) / 50000U));
  }
  if ((uv >= 1800000U) && (uv <= 3300000U) && (((uv - 1800000U) % 100000U) == 0U)) {
    return Result<uint8_t>::Success(static_cast<uint8_t>(16U + ((uv - 1800000U) / 100000U)));
  }
  return Result<uint8_t>::Failure(Error::InvalidArgument);
}

/**
 * @brief The voltage of an LDO code.
 * @param rail The LDO.
 * @param code The register code (5 bits for LDO1 and LDO3, 4 bits for LDO2).
 * @return uV; 0 for a rail that is not an LDO.
 */
constexpr uint32_t LdoUv(Rail rail, uint32_t code) noexcept {
  if (rail == Rail::Ldo2) {
    return 1800000U + (100000U * (code & 0x0FU));
  }
  if ((rail != Rail::Ldo1) && (rail != Rail::Ldo3)) {
    return 0U;
  }
  const uint32_t c = code & 0x1FU;
  return (c < 16U) ? (750000U + (50000U * c)) : (1800000U + (100000U * (c - 16U)));
}

/**
 * @brief VCOIN code for a coin cell charge voltage (1.8 V + 100 mV x code, ambiguity C40).
 * @param mv 1800 to 3300 in 100 mV steps.
 * @return The code 0 to 15, or `InvalidArgument`.
 */
constexpr Result<uint8_t> CoinVoltageCode(uint32_t mv) noexcept {
  if ((mv < 1800U) || (mv > 3300U) || (((mv - 1800U) % 100U) != 0U)) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint8_t>::Success(static_cast<uint8_t>((mv - 1800U) / 100U));
}

/**
 * @brief The 4-bit power-state code of STATE_INFO (Table 146) as a `PowerState`.
 * @param code STATE_INFO[5:0].
 * @return The state; `Unknown` for any other code.
 */
constexpr PowerState DecodePowerState(uint32_t code) noexcept {
  switch (code) {
  case 0x00U:
    return PowerState::Wait;
  case 0x0CU:
    return PowerState::Run;
  case 0x0DU:
    return PowerState::Standby;
  case 0x0EU:
    return PowerState::Sleep;
  case 0x2BU:
    return PowerState::RegsDisable;
  default:
    break;
  }
  return PowerState::Unknown;
}

/**
 * @brief The option an OTP_FLAVOR byte names (Table 87 lists 0 to 3 and then "continues": a larger
 * byte is `Unknown`, ambiguity C10, and the part's Table 85 profile is not assumed for it).
 * @param flavor OTP_FLAVOR.
 * @return The option, or `Unknown`.
 */
constexpr Option DecodeOption(uint32_t flavor) noexcept {
  return (flavor <= 3U) ? static_cast<Option>(flavor) : Option::Unknown;
}

// The checks sit in `detail` for its typed integer literals (MISRA C++:2023 Rule 7.0.6).
namespace detail {

static_assert((ChargeVoltageCode(4200_u32).value == 43U) &&
              (ChargeVoltageCode(3500_u32).value == 8U) &&
              (ChargeVoltageCode(4440_u32).value == 55U) && !ChargeVoltageCode(4460_u32).ok() &&
              !ChargeVoltageCode(3490_u32).ok() && !ChargeVoltageCode(4210_u32).ok());
static_assert((ChargeVoltageMv(43_u32) == 4200U) && (ChargeVoltageMv(0_u32) == 3500U) &&
              (ChargeVoltageMv(63_u32) == 4440U));
static_assert((ChargeCurrentCode(100_u32).value == 0U) &&
              (ChargeCurrentCode(1000_u32).value == 18U) && !ChargeCurrentCode(1050_u32).ok() &&
              !ChargeCurrentCode(125_u32).ok());
static_assert(!ChargeCurrentMa(19_u32).ok() && (ChargeCurrentMa(18_u32).value == 1000U));
static_assert((TerminationCode(50_u32).value == 4U) && !TerminationCode(40_u32).ok() &&
              !TerminationMa(5_u32).ok());
static_assert((InputLimitCode(1500_u32).value == 20U) && (InputLimitCode(400_u32).value == 13U) &&
              !InputLimitCode(2000_u32).ok() && !InputLimitMa(21_u32).ok());
static_assert((BuckDvsCode(612500_u32).value == 1U) && (BuckDvsCode(1387500_u32).value == 63U) &&
              !BuckDvsCode(1400000_u32).ok() && !BuckDvsCode(605000_u32).ok());
static_assert((BuckDvsUv(63_u32) == 1387500U) && (BuckFixedUvOf(6_u32) == 3000000U) &&
              (BuckFixedUvOf(7_u32) == 3300000U) && (BuckFixedUvOf(63_u32) == 3300000U));
static_assert((Sw3Uv(0_u32) == 1800000U) && (Sw3Uv(15_u32) == 3300000U));
static_assert((LdoCode(Rail::Ldo1, 750000_u32).value == 0U) &&
              (LdoCode(Rail::Ldo1, 1500000_u32).value == 15U) &&
              (LdoCode(Rail::Ldo3, 1800000_u32).value == 16U) &&
              (LdoCode(Rail::Ldo3, 3300000_u32).value == 31U) &&
              !LdoCode(Rail::Ldo1, 1600000_u32).ok() && !LdoCode(Rail::Ldo2, 1700000_u32).ok() &&
              (LdoCode(Rail::Ldo2, 3300000_u32).value == 15U) &&
              !LdoCode(Rail::Sw1, 1800000_u32).ok());
static_assert((LdoUv(Rail::Ldo1, 15_u32) == 1500000U) && (LdoUv(Rail::Ldo1, 16_u32) == 1800000U) &&
              (LdoUv(Rail::Ldo2, 15_u32) == 3300000U) && (LdoUv(Rail::Sw1, 0_u32) == 0U));
static_assert((CoinVoltageCode(3300_u32).value == 15U) && !CoinVoltageCode(1750_u32).ok());
static_assert((DecodePowerState(0x0C_u32) == PowerState::Run) &&
              (DecodePowerState(0x2B_u32) == PowerState::RegsDisable) &&
              (DecodePowerState(0x01_u32) == PowerState::Unknown));

/** @} */

} // namespace detail

} // namespace pf1550

#endif // PF1550_CODEC_HPP
