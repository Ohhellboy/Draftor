#pragma once

#include <Arduino.h>
#include <SPI.h>

#include <cstddef>
#include <cstdint>

#include "st7306_osptek_lpm8_adapter.hpp"

class ST7306OsptekLpm8 {
 public:
  using Color = st7306_osptek_lpm8::Color;

  static constexpr int Width = st7306_osptek_lpm8::kPanelWidth;
  static constexpr int Height = st7306_osptek_lpm8::kPanelHeight;
  static constexpr std::size_t BufferSize =
      st7306_osptek_lpm8::kFramebufferSize;
  static constexpr std::uint32_t SpiHz = 10000000UL;

  ST7306OsptekLpm8(SPIClassRP2040& spi, std::uint8_t sck_pin,
                   std::uint8_t mosi_pin, std::uint8_t miso_pin,
                   std::uint8_t cs_pin, std::uint8_t dc_pin,
                   std::uint8_t reset_pin);

  bool begin();
  void fill(Color color);
  void pixel(int x, int y, Color color);
  Color pixelAt(int x, int y) const;
  void fillRect(int x, int y, int width, int height, Color color);

  // The donor has one device-passed physical write path: a full-frame update.
  // Synchronous completion is the success boundary used by panel shadow.
  bool update();

  // Device-passed full-width native rows; the framebuffer stream is unchanged.
  bool updateNativeRows(std::uint16_t first_native_row,
                        std::uint16_t last_native_row);

  const std::uint8_t* framebufferData() const { return framebuffer_; }
  std::size_t framebufferSize() const { return BufferSize; }
  std::uint32_t bufferCrc32() const;

 private:
  SPIClassRP2040& spi_;
  std::uint8_t sck_pin_;
  std::uint8_t mosi_pin_;
  std::uint8_t miso_pin_;
  std::uint8_t cs_pin_;
  std::uint8_t dc_pin_;
  std::uint8_t reset_pin_;
  bool begun_ = false;
  std::uint8_t framebuffer_[BufferSize]{};

  void hardwareReset();
  void initPanel();
  void transaction(bool is_data, const std::uint8_t* bytes,
                   std::size_t size);
  void command(std::uint8_t value);
  void data(const std::uint8_t* values, std::size_t size);
  void setFullWindow();
};
