/**
 * @file pf1550_irq.hpp
 * @brief Interrupt map of the PF1550: which register and bit holds each source, and helpers over
 * the events `ServiceInterrupts()` returns.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 */
#ifndef PF1550_IRQ_HPP
#define PF1550_IRQ_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "pf1550_detail.hpp"
#include "pf1550_registers.hpp"
#include "pf1550_types.hpp"

namespace pf1550 {

/**
 * @defgroup pf1550_irq Interrupt map
 * @brief Source locations, masks and fault classification.
 * @{
 */

/// Where one interrupt source lives.
struct IrqLocation {
  IrqGroup group; ///< The group (one status, mask and sense register).
  uint8_t bit;    ///< Bit number in those registers.
};

/// The location of every source, indexed by `IrqSource` (Tables 90 to 110 and 150).
inline constexpr std::array<IrqLocation, kIrqSourceCount> kIrqLocations = {{
    {.group = IrqGroup::Charger, .bit = 0},    {.group = IrqGroup::Charger, .bit = 1},
    {.group = IrqGroup::Charger, .bit = 2},    {.group = IrqGroup::Charger, .bit = 3},
    {.group = IrqGroup::Charger, .bit = 5},    {.group = IrqGroup::Charger, .bit = 6},
    {.group = IrqGroup::Charger, .bit = 7},    {.group = IrqGroup::SwLowSide, .bit = 0},
    {.group = IrqGroup::SwLowSide, .bit = 1},  {.group = IrqGroup::SwLowSide, .bit = 2},
    {.group = IrqGroup::SwHighSide, .bit = 0}, {.group = IrqGroup::SwHighSide, .bit = 1},
    {.group = IrqGroup::SwHighSide, .bit = 2}, {.group = IrqGroup::SwDvs, .bit = 0},
    {.group = IrqGroup::SwDvs, .bit = 1},      {.group = IrqGroup::Ldo, .bit = 0},
    {.group = IrqGroup::Ldo, .bit = 1},        {.group = IrqGroup::Ldo, .bit = 2},
    {.group = IrqGroup::Temp, .bit = 0},       {.group = IrqGroup::Temp, .bit = 2},
    {.group = IrqGroup::Onkey, .bit = 0},      {.group = IrqGroup::Onkey, .bit = 1},
    {.group = IrqGroup::Onkey, .bit = 2},      {.group = IrqGroup::Onkey, .bit = 3},
    {.group = IrqGroup::Onkey, .bit = 4},      {.group = IrqGroup::Onkey, .bit = 5},
    {.group = IrqGroup::Misc, .bit = 0},       {.group = IrqGroup::Misc, .bit = 1},
    {.group = IrqGroup::Misc, .bit = 2},       {.group = IrqGroup::Misc, .bit = 3},
    {.group = IrqGroup::Misc, .bit = 4},
}};

/// The status register of each group, indexed by `IrqGroup`.
inline constexpr std::array<uint8_t, kIrqGroups> kIrqStatRegs = {
    reg::kChgInt,      reg::kSwIntStat0,   reg::kSwIntStat1,    reg::kSwIntStat2,
    reg::kLdoIntStat0, reg::kTempIntStat0, reg::kOnkeyIntStat0, reg::kMiscIntStat0};
/// The mask register of each group.
inline constexpr std::array<uint8_t, kIrqGroups> kIrqMaskRegs = {
    reg::kChgIntMask,  reg::kSwIntMask0,   reg::kSwIntMask1,    reg::kSwIntMask2,
    reg::kLdoIntMask0, reg::kTempIntMask0, reg::kOnkeyIntMask0, reg::kMiscIntMask0};
/// The sense register of each group (the charger's is `CHG_INT_OK`).
inline constexpr std::array<uint8_t, kIrqGroups> kIrqSenseRegs = {
    reg::kChgIntOk,     reg::kSwIntSense0,   reg::kSwIntSense1,    reg::kSwIntSense2,
    reg::kLdoIntSense0, reg::kTempIntSense0, reg::kOnkeyIntSense0, reg::kMiscIntSense0};
/// The bits that carry a source in each group; the others are reserved and preserved.
inline constexpr std::array<uint8_t, kIrqGroups> kIrqDefinedBits = {0xEF, 0x07, 0x07, 0x03,
                                                                    0x07, 0x05, 0x3F, 0x1F};

/// INT_CATEGORY bit of a group's summary (the three SW groups share bits 1 to 3 by rail).
inline constexpr uint8_t kCategoryCharger = 1U << 0;
/// SW1_INT, SW2_INT and SW3_INT.
inline constexpr uint8_t kCategorySw = 0x0E;
/// LDO_INT.
inline constexpr uint8_t kCategoryLdo = 1U << 4;
/// ONKEY_INT.
inline constexpr uint8_t kCategoryOnkey = 1U << 5;
/// TEMP_INT.
inline constexpr uint8_t kCategoryTemp = 1U << 6;
/// MISC_INT.
inline constexpr uint8_t kCategoryMisc = 1U << 7;

/// The INT_CATEGORY bits that summarise a group.
/// @param group The group.
/// @return The summary bits; 0 for a value that is not an enumerator.
constexpr uint8_t CategoryBitsOf(IrqGroup group) noexcept {
  switch (group) {
  case IrqGroup::Charger:
    return kCategoryCharger;
  case IrqGroup::SwLowSide:
  case IrqGroup::SwHighSide:
    return kCategorySw;
  case IrqGroup::SwDvs:
    return 0x06;
  case IrqGroup::Ldo:
    return kCategoryLdo;
  case IrqGroup::Temp:
    return kCategoryTemp;
  case IrqGroup::Onkey:
    return kCategoryOnkey;
  case IrqGroup::Misc:
    return kCategoryMisc;
  default:
    break;
  }
  return 0U;
}

/// The location of a source.
/// @param source The source.
/// @return Its group and bit; group `Charger`, bit 0 for a value that is not an enumerator.
constexpr IrqLocation LocationOf(IrqSource source) noexcept {
  const auto i = static_cast<std::size_t>(detail::ToUnderlying(source));
  return (i < kIrqLocations.size()) ? kIrqLocations[i]
                                    : IrqLocation{.group = IrqGroup::Charger, .bit = 0};
}

/// Whether a source is set in the events (the status bytes as read, before the clear).
/// @param events What `ServiceInterrupts()` returned.
/// @param source The source.
/// @return true when set.
constexpr bool Has(const InterruptEvents& events, IrqSource source) noexcept {
  const IrqLocation loc = LocationOf(source);
  const uint32_t byte =
      events.status[static_cast<std::size_t>(detail::ToUnderlying(loc.group)) % kIrqGroups];
  return ((byte >> (loc.bit & 7U)) & 1U) != 0U;
}

/// Whether a source reports a fault of a rail or of the die: a current limit, an LDO fault, 125 C
/// or a system overvoltage.
/// @param source The source.
/// @return true for a fault-class source.
constexpr bool IsFaultSource(IrqSource source) noexcept {
  // SW1..3 low-side (7..9) and high-side (10..12) limits, LDO faults (15..17), 125 C (19), SYS_OVLO
  // (30).
  constexpr uint32_t kFaultSources = (0x3FU << 7) | (0x7U << 15) | (1U << 19) | (1U << 30);
  const auto i = static_cast<uint32_t>(detail::ToUnderlying(source));
  return (i < kIrqSourceCount) && (((kFaultSources >> i) & 1U) != 0U);
}

/// Whether any fault-class source is set.
/// @param status One status byte per group.
/// @return true when one is set.
constexpr bool HasFault(const std::array<uint8_t, kIrqGroups>& status) noexcept {
  InterruptEvents e{};
  e.status = status;
  for (std::size_t i = 0U; i < kIrqSourceCount; ++i) {
    const auto source = static_cast<IrqSource>(i);
    if (IsFaultSource(source) && Has(e, source)) {
      return true;
    }
  }
  return false;
}

/// The mask byte of a group.
/// @param mask The mask.
/// @param group The group.
/// @return The byte as stored in `mask`.
constexpr uint8_t MaskByteFor(const IrqMask& mask, IrqGroup group) noexcept {
  return mask.masked[static_cast<std::size_t>(detail::ToUnderlying(group)) % kIrqGroups];
}

/// A mask that unmasks exactly the sources in the list and masks every other.
/// @param sources The sources to unmask.
/// @return The mask.
template <std::size_t N>
constexpr IrqMask UnmaskOnly(const std::array<IrqSource, N>& sources) noexcept {
  IrqMask m{};
  for (const IrqSource s : sources) {
    const IrqLocation loc = LocationOf(s);
    auto& byte = m.masked[static_cast<std::size_t>(detail::ToUnderlying(loc.group)) % kIrqGroups];
    byte = static_cast<uint8_t>(static_cast<uint32_t>(byte) & ~(1U << (loc.bit & 7U)));
  }
  return m;
}

static_assert((LocationOf(IrqSource::ChgVbus).bit == 5U) &&
              (LocationOf(IrqSource::SysOvervoltage).bit == 4U));
static_assert((LocationOf(IrqSource::Temp125).group == IrqGroup::Temp) &&
              (LocationOf(IrqSource::Temp125).bit == 2U));
static_assert(IsFaultSource(IrqSource::Ldo2Fault) && !IsFaultSource(IrqSource::PowerUp));
static_assert(UnmaskOnly(std::array<IrqSource, 1>{IrqSource::OnkeyPush}).masked[6] == 0xFEU);

/** @} */

} // namespace pf1550

#endif // PF1550_IRQ_HPP
