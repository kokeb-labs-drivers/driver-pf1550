/**
 * @file esp32_pf1550_port.hpp
 * @brief ESP-IDF port for the PF1550 driver: i2c_master for the bus, GPIO for STANDBY and INTB,
 * esp_rom delays, the bus failure report and ESP_LOG for the log hook.
 *
 * Start from this file for any ESP-IDF target. The pins are in Esp32Pf1550Pins; the defaults match
 * the wiring table in examples/esp32/README.md (ESP32-C6 DevKitC-1).
 */
#ifndef PF1550_ESP32_PF1550_PORT_HPP
#define PF1550_ESP32_PF1550_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "pf1550.hpp"

/// The port's default SCL clock, checked against the driver's bus requirements (KB-CPP-123).
inline constexpr uint32_t kEsp32Pf1550ClockHz = 400'000;
static_assert(kEsp32Pf1550ClockHz <= pf1550::kMaxClockHz, "SCL above the PF1550's maximum");
static_assert(pf1550::kRepeatedStart, "this port reads with i2c_master_transmit_receive");

/// Pins and bus settings of the ESP-IDF port.
struct Esp32Pf1550Pins {
  i2c_port_num_t port = I2C_NUM_0;
  gpio_num_t sda = GPIO_NUM_6;
  gpio_num_t scl = GPIO_NUM_7;
  gpio_num_t standby = GPIO_NUM_10; ///< STANDBY pin of the chip; GPIO_NUM_NC when it is wired low.
  gpio_num_t intb = GPIO_NUM_11; ///< INTB pin of the chip (open drain); GPIO_NUM_NC when not wired.
  uint32_t clock_hz = kEsp32Pf1550ClockHz; ///< Up to `pf1550::kMaxClockHz`.
  int timeout_ms = 10;                     ///< Per transaction.
};

/// The PF1550 I2C port on ESP-IDF, with every optional hook.
class Esp32Pf1550Port : public pf1550::I2cInterface<Esp32Pf1550Port> {
public:
  explicit Esp32Pf1550Port(const Esp32Pf1550Pins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (bus_ != nullptr) {
      return true;
    }
    i2c_master_bus_config_t bus{};
    bus.i2c_port = pins_.port;
    bus.sda_io_num = pins_.sda;
    bus.scl_io_num = pins_.scl;
    bus.clk_source = I2C_CLK_SRC_DEFAULT;
    bus.glitch_ignore_cnt = 7;
    bus.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus, &bus_) != ESP_OK) {
      bus_ = nullptr;
      return false;
    }
    if (pins_.standby != GPIO_NUM_NC) {
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.standby);
      io.mode = GPIO_MODE_OUTPUT;
      // STANDBY is driven to its inactive level (low) before its output is enabled, so configuring
      // the pin never requests STANDBY (handbook KB-HW-018).
      if ((gpio_set_level(pins_.standby, 0) != ESP_OK) || (gpio_config(&io) != ESP_OK)) {
        return false;
      }
    }
    if (pins_.intb != GPIO_NUM_NC) {
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.intb);
      io.mode = GPIO_MODE_INPUT;
      io.pull_up_en =
          GPIO_PULLUP_ENABLE; // INTB is open drain; add an external pull-up for long wires
      if (gpio_config(&io) != ESP_OK) {
        return false;
      }
    }
    return true;
  }

  bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept {
    i2c_master_dev_handle_t dev = device(addr);
    return (dev != nullptr) && note(i2c_master_transmit(dev, data, len, pins_.timeout_ms));
  }

  bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                 std::size_t rx_len) noexcept {
    i2c_master_dev_handle_t dev = device(addr);
    return (dev != nullptr) &&
           note(i2c_master_transmit_receive(dev, tx, tx_len, rx, rx_len, pins_.timeout_ms));
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  /// STANDBY is active high at the chip's reset value (`STANDBYINV` = 0); the port maps the
  /// polarity.
  bool GpioSet(pf1550::CtrlPin pin, pf1550::GpioSignal signal) noexcept {
    if ((pin != pf1550::CtrlPin::Standby) || (pins_.standby == GPIO_NUM_NC)) {
      return false;
    }
    const uint32_t level = signal == pf1550::GpioSignal::ACTIVE ? 1 : 0;
    return gpio_set_level(pins_.standby, level) == ESP_OK;
  }

  /// INTB is open drain and active low: asserted means the pin reads low.
  bool GpioGet(pf1550::CtrlPin pin, bool& asserted) noexcept {
    if ((pin != pf1550::CtrlPin::Intb) || (pins_.intb == GPIO_NUM_NC)) {
      return false;
    }
    asserted = gpio_get_level(pins_.intb) == 0;
    return true;
  }

  /// What the last failed transfer was: a missing acknowledge is a NACK, anything else a bus fault.
  /// The mapping of ESP-IDF error codes is not verified on hardware.
  bool GetLastFailure(pf1550::I2cFailure& why) noexcept {
    why = last_ == ESP_OK                     ? pf1550::I2cFailure::None
          : last_ == ESP_ERR_INVALID_RESPONSE ? pf1550::I2cFailure::DataNack
                                              : pf1550::I2cFailure::Bus;
    return true;
  }

  void Log(pf1550::LogLevel level, const char* message) noexcept {
    const esp_log_level_t esp_level = level == pf1550::LogLevel::Error     ? ESP_LOG_ERROR
                                      : level == pf1550::LogLevel::Warning ? ESP_LOG_WARN
                                      : level == pf1550::LogLevel::Info    ? ESP_LOG_INFO
                                                                           : ESP_LOG_DEBUG;
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  bool note(esp_err_t err) noexcept {
    last_ = err;
    return err == ESP_OK;
  }

  // The device handle for one 7-bit address, created on first use (one chip per port in this
  // example).
  i2c_master_dev_handle_t device(uint8_t addr) noexcept {
    if ((device_ != nullptr) && (device_addr_ == addr)) {
      return device_;
    }
    if (device_ != nullptr) {
      i2c_master_bus_rm_device(device_);
      device_ = nullptr;
    }
    i2c_device_config_t cfg{};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = addr;
    cfg.scl_speed_hz = pins_.clock_hz;
    if (i2c_master_bus_add_device(bus_, &cfg, &device_) != ESP_OK) {
      device_ = nullptr;
      return nullptr;
    }
    device_addr_ = addr;
    return device_;
  }

  static constexpr const char* kTag = "pf1550";
  Esp32Pf1550Pins pins_;
  i2c_master_bus_handle_t bus_ = nullptr;
  i2c_master_dev_handle_t device_ = nullptr;
  uint8_t device_addr_ = 0;
  esp_err_t last_ = ESP_OK;
};

#endif // PF1550_ESP32_PF1550_PORT_HPP
