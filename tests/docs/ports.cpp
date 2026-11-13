/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "pf1550.hpp"

bool platform_i2c_init() noexcept;
bool platform_i2c_write(uint8_t addr7, const uint8_t* data, std::size_t len) noexcept;
bool platform_i2c_write_read(uint8_t addr7, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                             std::size_t rx_len) noexcept;
void platform_delay_us(uint32_t us) noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
bool platform_gpio_read(int pin) noexcept;
pf1550::I2cFailure platform_i2c_last_failure() noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public pf1550::I2cInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_i2c_init();
  }
  bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept {
    return platform_i2c_write(addr, data, len);
  }
  bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                 std::size_t rx_len) noexcept {
    return platform_i2c_write_read(addr, tx, tx_len, rx, rx_len);
  }

  // Required by the DVS wait.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: the STANDBY pin on a GPIO. ACTIVE means "standby requested"; with STANDBYINV = 0 (the
  // default) that is a high level.
  bool GpioSet(pf1550::CtrlPin pin, pf1550::GpioSignal signal) noexcept {
    if (pin != pf1550::CtrlPin::Standby) {
      return false;
    }
    return platform_gpio_write(kStandbyGpio, signal == pf1550::GpioSignal::ACTIVE);
  }

  // Optional: INTB, open drain. "Asserted" means the pin is low.
  bool GpioGet(pf1550::CtrlPin pin, bool& asserted) noexcept {
    if (pin != pf1550::CtrlPin::Intb) {
      return false;
    }
    asserted = !platform_gpio_read(kIntbGpio);
    return true;
  }

  // Optional: tells an absent chip (nothing acknowledged the address) from a bus error.
  bool GetLastFailure(pf1550::I2cFailure& why) noexcept {
    why = platform_i2c_last_failure();
    return true;
  }

  // Optional: one line per lifecycle event or failure. The driver never formats; the port does.
  void Log(pf1550::LogLevel level, const char* message) noexcept {
    if (level > pf1550::LogLevel::Warning) {
      return;
    }
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kStandbyGpio = 7;
  static constexpr int kIntbGpio = 8;
};
// --8<-- [end:full-port]

// --8<-- [start:standby]
pf1550::Error EnterStandby(pf1550::PF1550<FullPort>& pmic) {
  // With GpioSet() this drives the STANDBY pin; a port without it gets Error::Unsupported.
  return pmic.RequestStandby(true).error;
}
// --8<-- [end:standby]
