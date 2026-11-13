/**
 * @file pf1550_sequencer.hpp
 * @brief The regulator power-up and power-down sequencer of the PF1550 as pure `constexpr` data:
 * the pre-programmed start-up options (Table 85), the sequencer timing (Table 84) and the
 * power-down order (section 10.7).
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The power-up order is fixed by OTP and cannot be read back over I2C (the OTP image is not
 * documented in the public data sheet), so the driver reports it only for the pre-programmed
 * options A1 to A9 that Table 85 lists, selected by the `OTP_FLAVOR` byte that `ReadVariant()`
 * returns. The power-down order is a set of read/write registers, `*_PWRDN_SEQ`, that the
 * application may change in a system-on state.
 *
 * Model choices where the data sheet is silent (each one has a knob in the test model):
 * - the slot of a regulator with `PWRUP_SEQ` = n starts (n - 1) slots after the first one, and a
 *   `PWRUP_SEQ` of 0 starts in the first slot (ambiguity C54);
 * - regulators with equal power-down order turn off together (C55);
 * - with some orders non-zero, a regulator with order 0 turns off last (C56).
 */
#ifndef PF1550_SEQUENCER_HPP
#define PF1550_SEQUENCER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_detail.hpp"
#include "pf1550_types.hpp"

namespace pf1550 {

/**
 * @defgroup pf1550_sequencer Sequencer
 * @brief Start-up options, sequencer timing and the power-down schedule.
 * @{
 */

/// Turn-on delay of VSNVS after a cold start, microseconds (Table 84, tD1 typical 0.6 ms).
inline constexpr uint32_t kVsnvsTurnOnDelayUs = 600;
/// Rise time of VSNVS, microseconds (tR1 typical 0.1 ms).
inline constexpr uint32_t kVsnvsRiseUs = 100;
/// Power-up slot with `OTP_SEQ_CLK_SPEED` = 0, microseconds (tD3 typical 0.5 ms).
inline constexpr uint32_t kSlotFastUs = 500;
/// Power-up slot with `OTP_SEQ_CLK_SPEED` = 1, microseconds (tD3 typical 2.0 ms).
inline constexpr uint32_t kSlotSlowUs = 2000;
/// Power-down delay between regulators, microseconds (tD5 typical 2.0 ms).
inline constexpr uint32_t kPowerDownStepUs = 2000;
/// Largest sequence number a `*_PWRUP_SEQ` or `*_PWRDN_SEQ` field holds.
inline constexpr uint8_t kMaxSequenceNumber = 7;

/** @brief The start-up behaviour of one pre-programmed option (Table 85). */
struct StartupProfile {
  /// `OTP_xxx_PWRUP_SEQ[2:0]` per rail, indexed by `Rail`.
  std::array<uint8_t, kRailCount> order{};
  /// Length of one power-up slot in microseconds: 500 for `OTP_SEQ_CLK_SPEED` = 0, 2000 for 1.
  uint32_t slot_us{0};
  /// `OTP_PWRON_CFG`: true when PWRON is edge sensitive (option A8), false for level.
  bool pwron_edge{false};
  /// `OTP_SW1_DVS_SEL`: true for DVS mode. The merged cells of Table 85 are read by position
  /// (ambiguity C57).
  bool sw1_dvs{false};
  /// `OTP_SW2_DVS_SEL`: true for DVS mode (C57).
  bool sw2_dvs{false};
};

namespace detail {

/// Table 85 rows of the power-up sequence numbers, one per option A1 to A9; columns are SW1, SW2,
/// SW3, LDO1, LDO2, LDO3 and VREFDDR.
inline constexpr std::array<std::array<uint8_t, kRailCount>, 9> kPowerUpOrder = {{
    {1, 2, 3, 4, 4, 5, 5}, // A1
    {5, 5, 1, 1, 1, 1, 5}, // A2
    {3, 3, 3, 3, 2, 3, 3}, // A3
    {4, 3, 2, 1, 2, 1, 3}, // A4
    {3, 3, 3, 3, 2, 3, 3}, // A5
    {3, 3, 3, 3, 2, 3, 3}, // A6
    {3, 3, 3, 3, 2, 3, 3}, // A7
    {3, 3, 3, 3, 2, 3, 3}, // A8
    {3, 3, 3, 3, 3, 3, 3}, // A9
}};

/// Table 85 `OTP_SW1_DVS_SEL` per option A1 to A9: true for DVS mode.
inline constexpr std::array<bool, 9> kSw1Dvs = {false, true, true, true, true,
                                                true,  true, true, false};
/// Table 85 `OTP_SW2_DVS_SEL` per option A1 to A9: true for DVS mode.
inline constexpr std::array<bool, 9> kSw2Dvs = {true,  false, false, true, false,
                                                false, false, false, false};

} // namespace detail

/**
 * @brief The pre-programmed start-up sequence of an option (Table 85).
 * @param option `A1` to `A9`.
 * @return The profile, or `InvalidArgument` for `A0` (not programmed: nothing is assumed) and
 *         `Unknown` (a flavour byte the data sheet does not list).
 */
constexpr Result<StartupProfile> StartupProfileOf(Option option) noexcept {
  const auto n = static_cast<std::size_t>(detail::ToUnderlying(option));
  if ((n < 1U) || (n > detail::kPowerUpOrder.size())) {
    return Result<StartupProfile>::Failure(Error::InvalidArgument);
  }
  StartupProfile p{};
  p.order = detail::kPowerUpOrder[n - 1U];
  p.slot_us = (option == Option::A1) ? kSlotFastUs : kSlotSlowUs;
  p.pwron_edge = (option == Option::A8);
  p.sw1_dvs = detail::kSw1Dvs[n - 1U];
  p.sw2_dvs = detail::kSw2Dvs[n - 1U];
  return Result<StartupProfile>::Success(p);
}

/**
 * @brief When a rail turns on, counted from the start of the first power-up slot.
 * @param profile The start-up profile.
 * @param rail The rail.
 * @return Microseconds: (order - 1) slots, 0 for order 0 and 1 (ambiguity C54).
 */
constexpr uint32_t PowerUpDelayUs(const StartupProfile& profile, Rail rail) noexcept {
  const uint32_t order =
      profile.order[static_cast<std::size_t>(detail::ToUnderlying(rail)) % kRailCount];
  return (order > 1U) ? ((order - 1U) * profile.slot_us) : 0U;
}

/**
 * @brief How long the power-up sequencer runs: the start of the last slot used.
 * @param profile The start-up profile.
 * @return Microseconds from the first slot to the last regulator turning on.
 */
constexpr uint32_t PowerUpDurationUs(const StartupProfile& profile) noexcept {
  uint32_t longest = 0U;
  for (const uint8_t order : profile.order) {
    const uint32_t d = (order > 1U) ? ((static_cast<uint32_t>(order) - 1U) * profile.slot_us) : 0U;
    longest = (d > longest) ? d : longest;
  }
  return longest;
}

/**
 * @brief The delay of RESETBMCU after the last regulator, from `POR_DLY[2:0]` (Table 135).
 * @param code `POR_DLY`: 0 is 2 ms, 2 to 7 are 4, 8, 16, 128, 256 and 1024 ms; 1 is not defined.
 * @return Milliseconds, or `InvalidArgument` for code 1 and for a code above 7.
 */
constexpr Result<uint32_t> ResetReleaseDelayMs(uint32_t code) noexcept {
  constexpr std::array<uint32_t, 8> kMs = {2, 0, 4, 8, 16, 128, 256, 1024};
  if ((code >= kMs.size()) || (kMs[code] == 0U)) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint32_t>::Success(kMs[code]);
}

/**
 * @brief From a cold start to RESETBMCU high, for an option and a `POR_DLY` code (Table 84).
 * @param profile The start-up profile.
 * @param por_dly_code `POR_DLY[2:0]`.
 * @return Microseconds: VSNVS turn-on, the power-up sequencer, then the reset release delay; or
 *         `InvalidArgument` for an undefined `POR_DLY` code. The wait for PWRON (tD2) is the
 *         application's and not included.
 */
constexpr Result<uint32_t> ColdStartToResetReleaseUs(const StartupProfile& profile,
                                                     uint32_t por_dly_code) noexcept {
  const Result<uint32_t> ms = ResetReleaseDelayMs(por_dly_code);
  if (!ms.ok()) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  return Result<uint32_t>::Success(kVsnvsTurnOnDelayUs + PowerUpDurationUs(profile) +
                                   (ms.value * 1000U));
}

/**
 * @brief Whether the power-down sequencer is bypassed: every order is 0 (section 10.7).
 * @param sequence The `*_PWRDN_SEQ` values.
 * @return true when all regulators turn off at once.
 */
constexpr bool IsPowerDownBypassed(const PowerDownSequence& sequence) noexcept {
  bool bypassed = true;
  for (const uint8_t order : sequence.order) {
    bypassed = bypassed && (order == 0U);
  }
  return bypassed;
}

/**
 * @brief When a rail turns off in a power-down, counted from the start of the sequence.
 * @param sequence The `*_PWRDN_SEQ` values.
 * @param rail The rail.
 * @return Microseconds: 0 when the sequencer is bypassed, else (highest order - this order) steps
 *         of `kPowerDownStepUs`; regulators of equal order turn off together (C55) and order 0
 *         turns off last (C56).
 */
constexpr uint32_t PowerDownDelayUs(const PowerDownSequence& sequence, Rail rail) noexcept {
  uint32_t highest = 0U;
  for (const uint8_t order : sequence.order) {
    highest = (order > highest) ? order : highest;
  }
  const uint32_t own =
      sequence.order[static_cast<std::size_t>(detail::ToUnderlying(rail)) % kRailCount];
  return (highest - own) * kPowerDownStepUs;
}

/**
 * @brief How long the power-down sequence takes: the delay of the last rail.
 * @param sequence The `*_PWRDN_SEQ` values.
 * @return Microseconds; 0 when bypassed.
 */
constexpr uint32_t PowerDownDurationUs(const PowerDownSequence& sequence) noexcept {
  uint32_t highest = 0U;
  for (const uint8_t order : sequence.order) {
    highest = (order > highest) ? order : highest;
  }
  return highest * kPowerDownStepUs;
}

/**
 * @brief The power-down order that mirrors the power-up order of an option, as the registers hold
 * it after reset (section 10.7: `SW1_PWRDN_SEQ` = `OTP_SW1_PWRUP_SEQ`, and so on).
 * @param profile The start-up profile.
 * @return The default `PowerDownSequence`.
 */
constexpr PowerDownSequence MirrorOf(const StartupProfile& profile) noexcept {
  PowerDownSequence s{};
  s.order = profile.order;
  return s;
}

// The checks sit in `detail` for its typed integer literals (MISRA C++:2023 Rule 7.0.6).
namespace detail {

static_assert((StartupProfileOf(Option::A1).value.slot_us == 500U) &&
              (StartupProfileOf(Option::A4).value.slot_us == 2000U));
static_assert(!StartupProfileOf(Option::A0).ok() && !StartupProfileOf(Option::Unknown).ok());
static_assert((StartupProfileOf(Option::A4).value.order[0] == 4U) &&
              (StartupProfileOf(Option::A4).value.order[6] == 3U));
static_assert(StartupProfileOf(Option::A8).value.pwron_edge &&
              !StartupProfileOf(Option::A9).value.pwron_edge);
static_assert(StartupProfileOf(Option::A1).value.sw2_dvs &&
              !StartupProfileOf(Option::A1).value.sw1_dvs &&
              StartupProfileOf(Option::A5).value.sw1_dvs &&
              !StartupProfileOf(Option::A9).value.sw1_dvs);
static_assert(PowerUpDurationUs(StartupProfileOf(Option::A4).value) == 6000U);
static_assert(PowerUpDurationUs(StartupProfileOf(Option::A1).value) == 2000U);
static_assert((ResetReleaseDelayMs(0_u32).value == 2U) && !ResetReleaseDelayMs(1_u32).ok() &&
              (ResetReleaseDelayMs(7_u32).value == 1024U));
static_assert(ColdStartToResetReleaseUs(StartupProfileOf(Option::A4).value, 0_u32).value == 8600U);
static_assert(IsPowerDownBypassed(PowerDownSequence{}) &&
              (PowerDownDurationUs(PowerDownSequence{}) == 0U));
static_assert(PowerDownDelayUs(MirrorOf(StartupProfileOf(Option::A4).value), Rail::Sw1) == 0U);
static_assert(PowerDownDelayUs(MirrorOf(StartupProfileOf(Option::A4).value), Rail::Ldo1) == 6000U);

} // namespace detail

/** @} */

} // namespace pf1550

#endif // PF1550_SEQUENCER_HPP
