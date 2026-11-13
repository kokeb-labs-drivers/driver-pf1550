/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "pf1550.hpp"

namespace {

// A port that answers every read with 0x42: enough to instantiate the whole driver template.
class NullPort : public pf1550::I2cInterface<NullPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept {
    (void)addr;
    (void)data;
    (void)len;
    return true;
  }
  bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                 std::size_t rx_len) noexcept {
    (void)addr;
    (void)tx;
    (void)tx_len;
    for (std::size_t i = 0; i < rx_len; ++i) {
      rx[i] = 0x42; // not 0x00 (a stuck line, `Bus`), not 0xFF, not DEVICE_ID
    }
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

} // namespace

int main() {
  NullPort port;
  pf1550::Config config{};
  config.limits = pf1550::SingleCellLimits();
  pf1550::PF1550<NullPort> sensor(port, config);
  // DEVICE_ID reads 0x42, so EnsureInitialized() must fail with DeviceIdMismatch: proof the
  // template compiled and ran.
  const auto r = sensor.EnsureInitialized();
  return ((r.error == pf1550::Error::DeviceIdMismatch) &&
          (std::strlen(pf1550::GetDriverVersion()) > 0U))
             ? 0
             : 1;
}
