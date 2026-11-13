// clang-format off: the table below must keep one row per line.
/**
 * @file pf1550_i2c_interface.hpp
 * @brief The I2C port of the PF1550 driver: the CRTP base, its pin enums, the `I2cPort` concept
 * and the register framing.
 * @copyright Copyright (c) 2026 The driver-pf1550 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `pf1550::I2cInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the bus up on the first call; `true` at once afterwards |
 * | `bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept` | START, `addr` + W, `len` bytes, STOP; `false` on a NACK or bus error |
 * | `bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx, std::size_t rx_len) noexcept` | START, `addr` + W, `tx`, repeated START, `addr` + R, `rx_len` bytes, STOP |
 * | `void DelayUs(uint32_t us) noexcept` | Wait; needed by the DVS and charger waits |
 * | `bool GpioSet(CtrlPin pin, GpioSignal level) noexcept` (optional) | Drive `CtrlPin::Standby` |
 * | `bool GpioGet(CtrlPin pin, bool& asserted) noexcept` (optional) | Read `CtrlPin::Intb` (asserted = low) |
 * | `bool GetLastFailure(I2cFailure& why) noexcept` (optional) | Address NACK, data NACK or bus error |
 * | `void Log(LogLevel level, const char* message) noexcept` (optional) | One line per event |
 *
 * `addr` is the 7-bit address. The port moves raw bytes: the register address travels in the
 * payload, and the port never retries or configures the chip. What the chip needs from the bus is
 * below as `constexpr` values (`kI2cAddress`, `kMaxClockHz`, `kRepeatedStart`); a port checks its
 * settings against them with `static_assert`.
 */
// clang-format on
#ifndef PF1550_I2C_INTERFACE_HPP
#define PF1550_I2C_INTERFACE_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

#include "pf1550_detail.hpp"
#include "pf1550_types.hpp"

namespace pf1550 {

/** @brief Control pins the port may reach through the optional `GpioSet()` and `GpioGet()` hooks.
 */
enum class CtrlPin : uint8_t {
  Standby = 0, ///< STANDBY (pin 7), input of the chip: ACTIVE requests STANDBY (the port maps the
               ///< polarity; `STANDBYINV` = 0 means active high).
  Intb = 1,    ///< INTB (pin 9), open-drain output of the chip: `GpioGet()` reports asserted = low.
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/** @brief Why the last transfer failed, reported by the optional `GetLastFailure()` hook. */
enum class I2cFailure : uint8_t {
  None = 0,        ///< The last transfer succeeded.
  AddressNack = 1, ///< Nothing acknowledged the address: no chip, wrong address or unpowered.
  DataNack = 2,    ///< The address was acknowledged, a data byte was not.
  Bus = 3,         ///< Arbitration lost, a stuck line or a timeout.
};

namespace detail {
/// Last enumerator of `I2cFailure`.
template <>
struct EnumLast<I2cFailure> {
  static constexpr I2cFailure kValue = I2cFailure::Bus; ///< The last enumerator.
};
} // namespace detail

/**
 * @name Bus requirements
 * What the PF1550 needs from the I2C bus (datasheet sections 9.7.1, 9.7.2). A port and a board
 * configuration check their settings against these with `static_assert` (handbook KB-CPP-123).
 * @{
 */
/// Default 7-bit I2C address (section 9.7.1).
inline constexpr uint8_t kI2cAddress = 0x08;
/// Lowest address OTP can select (`I2C_ADDR[2:0]` = 000).
inline constexpr uint8_t kMinI2cAddress = 0x08;
/// Highest address OTP can select (`I2C_ADDR[2:0]` = 111).
inline constexpr uint8_t kMaxI2cAddress = 0x0F;
/// Highest SCL frequency: fast mode (section 9.7.2).
inline constexpr uint32_t kMaxClockHz = 400'000;
/// Shortest SCL and SDA rise and fall time the chip accepts, in nanoseconds (section 9.7).
inline constexpr uint32_t kMinEdgeNs = 20;
/// `true`: a register read is a combined write-read with a repeated START (Figure 25).
inline constexpr bool kRepeatedStart = true;
/** @} */

/**
 * @brief CRTP base of the platform I2C port.
 *
 * Derive `class MyPort : public pf1550::I2cInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data; the driver checks your class with the `I2cPort`
 * concept, so a missing or misdeclared hook is a compile error that names it.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class I2cInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  I2cInterface(const I2cInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  I2cInterface& operator=(const I2cInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  I2cInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~I2cInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `I2cInterface<Port>` and implements the required
 * hooks with exactly the types above, `noexcept` and returning `bool`.
 *
 * Each hook's address is matched with `detail::ExactHook`, which takes a member function of exactly
 * the signature and converts no value: a hook declared with a narrower length or without `noexcept`
 * does not satisfy the concept (handbook KB-CPP20-006).
 *
 * @tparam Port The port class.
 */
template <typename Port>
concept I2cPort = std::derived_from<Port, I2cInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(uint8_t, const uint8_t*, std::size_t) noexcept>::Take(&Port::Write);
  detail::ExactHook<Port, bool(uint8_t, const uint8_t*, std::size_t, uint8_t*,
                               std::size_t) noexcept>::Take(&Port::WriteRead);
};

namespace detail {

/// Longest register burst the driver issues, in data bytes: one. The data sheet does not say that
/// the address auto-increments (ambiguity C48), so every transfer is one register.
inline constexpr std::size_t kMaxBurst = 1;

/// A burst size the register helpers accept: 1 to `kMaxBurst` bytes, checked at compile time.
template <std::size_t N>
concept BurstSize = (N >= 1U) && (N <= kMaxBurst);

/// Register access over I2C: the register address is the first payload byte; reads use a repeated
/// START. One register per transfer.
template <I2cPort Port>
class RegisterBus {
public:
  RegisterBus(Port& port, uint8_t address) noexcept : port_(port), address_(address) {}

  Port& GetPort() & noexcept {
    return port_;
  }

  /// Read `out.size()` consecutive registers from `reg`; the size is checked at compile time.
  template <std::size_t N>
    requires BurstSize<N>
  Error Read(uint8_t reg, std::span<uint8_t, N> out) noexcept {
    return Read(reg, std::span<uint8_t>{out});
  }

  /// Read `out.size()` consecutive registers from `reg`. `InvalidArgument` (nothing sent) for 0 or
  /// more than `kMaxBurst` bytes, `Bus` on a NACK or bus error.
  Error Read(uint8_t reg, std::span<uint8_t> out) noexcept {
    if (out.empty() || (out.size() > kMaxBurst)) {
      return Error::InvalidArgument;
    }
    // The buffers the port sees are members of this object, never on the caller's stack, so a
    // port whose bus reads them by DMA reaches them (hardware lessons 20).
    pointer_[0] = reg;
    if (!port_.WriteRead(address_, pointer_.data(), pointer_.size(), rx_.data(), out.size())) {
      return Error::Bus;
    }
    for (std::size_t i = 0U; i < out.size(); ++i) {
      out[i] = rx_[i];
    }
    return Error::Ok;
  }

  /// Write `data.size()` consecutive registers from `reg`; the size is checked at compile time.
  template <std::size_t N>
    requires BurstSize<N>
  Error Write(uint8_t reg, std::span<const uint8_t, N> data) noexcept {
    return Write(reg, std::span<const uint8_t>{data});
  }

  /// Write `data.size()` consecutive registers from `reg`. `InvalidArgument` (nothing sent) for 0
  /// or more than `kMaxBurst` bytes, `Bus` on a NACK or bus error.
  Error Write(uint8_t reg, std::span<const uint8_t> data) noexcept {
    if (data.empty() || (data.size() > kMaxBurst)) {
      return Error::InvalidArgument;
    }
    frame_[0] = reg;
    const std::size_t count = data.size();
    for (std::size_t i = 0U; i < count; ++i) {
      frame_[1U + i] = data[i];
    }
    return port_.Write(address_, frame_.data(), count + 1U) ? Error::Ok : Error::Bus;
  }

private:
  Port& port_;
  uint8_t address_;
  std::array<uint8_t, 1> pointer_{}; ///< Register address of a read (DMA-reachable: a member).
  std::array<uint8_t, kMaxBurst> rx_{};
  std::array<uint8_t, 1U + kMaxBurst> frame_{};
};

} // namespace detail

} // namespace pf1550

#endif // PF1550_I2C_INTERFACE_HPP
