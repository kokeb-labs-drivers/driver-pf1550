/**
 * @file mock_pf1550_bus.hpp
 * @brief I2C mock ports over the register model. The transaction decoding here is the only
 * bus-specific part of the host tests.
 *
 * | Port | Hooks |
 * | --- | --- |
 * | `MinimalPort` | the required ones plus `DelayUs()` (needed by the DVS wait) |
 * | `FullPort` | also `GpioSet()` (STANDBY), `GpioGet()` (INTB), `GetLastFailure()` and `Log()` |
 */
#ifndef PF1550_MOCK_PF1550_BUS_HPP
#define PF1550_MOCK_PF1550_BUS_HPP

#include <bit>
#include <cstddef>
#include <cstdint>

#include "mock_pf1550.hpp"
#include "pf1550_i2c_interface.hpp"
#include "pf1550_types.hpp"

namespace pf1550_test {

/// Shared state of the mock ports: the chip, fault injection and counters.
struct MockBusState {
  Pf1550Model chip;
  uint8_t address = pf1550::kI2cAddress; ///< The address the chip answers.
  bool bus_up_ok = true;                 ///< EnsureInitialized() result.
  int fail_transfer = -1; ///< Fail the transaction with this index (0-based); -1 never.
  int fail_from = -1;     ///< Fail every transaction from this index on; -1 never.
  int transfers = 0;      ///< Transactions so far.
  int bus_up_calls = 0;   ///< EnsureInitialized() calls so far.
  pf1550::I2cFailure last_failure = pf1550::I2cFailure::None; ///< What GetLastFailure() reports.
  /// Read corruption (a floating or stuck line, a noisy bus): reads of `stuck_addr` return
  /// `stuck_value`; `flip_count` reads of `flip_addr`, after `flip_skip` clean ones, are XORed
  /// with `flip_mask`. The chip itself is not touched.
  uint32_t stuck_addr = 0x100U; ///< 0x100: none.
  uint8_t stuck_value = 0;
  uint32_t flip_addr = 0x100U; ///< 0x100: none.
  uint8_t flip_mask = 0;
  int flip_skip = 0;
  int flip_count = 0;
  /// The buffers the last transfers used (the driver must own them, hardware lessons 20).
  int reset_at = -1; ///< The chip resets (as a power loss does) before this transfer; -1 never.
  /// The driver object's address range: when set (`own_hi` > `own_lo`), a transfer whose buffer is
  /// outside it is counted in `outside` (hardware lessons 20: buffers belong to the driver).
  std::uintptr_t own_lo = 0;
  std::uintptr_t own_hi = 0;
  int outside = 0;
  const uint8_t* seen_tx = nullptr;
  const uint8_t* seen_rx = nullptr;
  /// After a read of `raise_after_addr` the chip raises one event: status register `raise_reg`, bit
  /// `raise_bit` (an interrupt that arrives between the read and the clear write).
  uint32_t raise_after_addr = 0x100U; ///< 0x100: none.
  uint8_t raise_reg = 0;
  uint8_t raise_bit = 0;

  void Own(const uint8_t* p) noexcept {
    const auto at = std::bit_cast<std::uintptr_t>(p);
    if ((own_hi > own_lo) && ((at < own_lo) || (at >= own_hi))) {
      ++outside;
    }
  }

  uint8_t Corrupt(uint8_t addr, uint8_t value) noexcept {
    if (static_cast<uint32_t>(addr) == stuck_addr) {
      return stuck_value;
    }
    if ((static_cast<uint32_t>(addr) == flip_addr) && (flip_count > 0)) {
      if (flip_skip > 0) {
        --flip_skip;
      } else {
        --flip_count;
        return static_cast<uint8_t>(static_cast<uint32_t>(value) ^ flip_mask);
      }
    }
    return value;
  }

  bool Fails(int index, uint8_t addr) noexcept {
    if (addr != address) {
      last_failure = pf1550::I2cFailure::AddressNack;
      return true;
    }
    if ((index == fail_transfer) || ((fail_from >= 0) && (index >= fail_from))) {
      last_failure = pf1550::I2cFailure::DataNack;
      return true;
    }
    last_failure = pf1550::I2cFailure::None;
    return false;
  }

  /// START, addr + W, [reg, data...], STOP.
  bool DoWrite(uint8_t addr, const uint8_t* data, std::size_t len) noexcept {
    const int index = transfers++;
    if (index == reset_at) {
      chip.Reset();
    }
    seen_tx = data;
    Own(data);
    if (Fails(index, addr) || (len == 0) || (data == nullptr)) {
      return false; // NACK
    }
    for (std::size_t i = 1; i < len; ++i) {
      chip.Write(static_cast<uint8_t>(data[0] + i - 1), data[i]);
    }
    return true;
  }

  /// START, addr + W, [reg], repeated START, addr + R, rx_len bytes, STOP.
  bool DoWriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                   std::size_t rx_len) noexcept {
    const int index = transfers++;
    if (index == reset_at) {
      chip.Reset();
    }
    seen_tx = tx;
    seen_rx = rx;
    Own(tx);
    Own(rx);
    if (Fails(index, addr) || (tx_len != 1) || (rx == nullptr)) {
      return false;
    }
    for (std::size_t i = 0; i < rx_len; ++i) {
      const auto at = static_cast<uint8_t>(tx[0] + i);
      rx[i] = Corrupt(at, chip.Read(at));
      if (static_cast<uint32_t>(at) == raise_after_addr) {
        raise_after_addr = 0x100U;
        chip.Raise(raise_reg, raise_bit);
      }
    }
    return true;
  }
};

/// The required I2C hooks over a MockBusState. Derive `class P : public MockPortBase<P>`.
template <typename Self>
class MockPortBase : public pf1550::I2cInterface<Self> {
public:
  MockBusState state;

  bool EnsureInitialized() noexcept {
    state.bus_up_calls++;
    return state.bus_up_ok;
  }
  bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept {
    return state.DoWrite(addr, data, len);
  }
  bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                 std::size_t rx_len) noexcept {
    return state.DoWriteRead(addr, tx, tx_len, rx, rx_len);
  }
};

/// Required hooks plus the delay; time passes in the model as the driver waits.
class MinimalPort : public MockPortBase<MinimalPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
};

/// Every optional hook, with counters.
class FullPort : public MockPortBase<FullPort> {
public:
  uint64_t delayed_us = 0;  ///< Sum of DelayUs() arguments.
  int log_lines = 0;        ///< Log() calls.
  int error_lines = 0;      ///< Log() calls at LogLevel::Error.
  bool gpio_ok = true;      ///< GpioSet() and GpioGet() result.
  bool standby_pin = false; ///< The STANDBY pin as driven.
  int gpio_gets = 0;        ///< GpioGet() calls.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
  bool GpioSet(pf1550::CtrlPin pin, pf1550::GpioSignal signal) noexcept {
    if (!gpio_ok || (pin != pf1550::CtrlPin::Standby)) {
      return false;
    }
    standby_pin = signal == pf1550::GpioSignal::ACTIVE;
    return true;
  }
  bool GpioGet(pf1550::CtrlPin pin, bool& asserted) noexcept {
    ++gpio_gets;
    if (!gpio_ok || (pin != pf1550::CtrlPin::Intb)) {
      return false;
    }
    asserted = state.chip.IntbAsserted();
    return true;
  }
  bool GetLastFailure(pf1550::I2cFailure& why) noexcept {
    why = state.last_failure;
    return true;
  }
  void Log(pf1550::LogLevel level, const char* message) noexcept {
    (void)message;
    log_lines++;
    if (level == pf1550::LogLevel::Error) {
      error_lines++;
    }
  }
};

} // namespace pf1550_test

#endif // PF1550_MOCK_PF1550_BUS_HPP
