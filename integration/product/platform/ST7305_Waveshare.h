#pragma once

#include <Arduino.h>
#include <SPI.h>

#include <cstddef>
#include <cstdint>

#include "st7305_waveshare_adapter.hpp"

#ifndef DRAFTOR_DISPLAY_TRANSACTION
#define DRAFTOR_DISPLAY_TRANSACTION 0
#endif

class ST7305Waveshare {
 public:
  using Color = st7305_waveshare::Color;

  static constexpr int Width = st7305_waveshare::Width;
  static constexpr int Height = st7305_waveshare::Height;
  static constexpr std::size_t BufferSize =
      st7305_waveshare::BufferSize;
  static constexpr std::uint32_t SpiHz = 10000000UL;

  ST7305Waveshare(SPIClass& spi, std::uint8_t sck_pin,
                   std::uint8_t mosi_pin, std::uint8_t cs_pin,
                   std::uint8_t dc_pin, std::uint8_t reset_pin);

  bool begin();
  void fill(Color color);
  void pixel(int x, int y, Color color);
  void fillRect(int x, int y, int width, int height, Color color);
  bool update();
  // Synchronous borrowed shadow: valid only for this call, never retained.
  // Its bytes must describe the last successfully completed panel memory.
  bool update(const std::uint8_t* completed, std::size_t size);
  struct TransferStats {
    std::uint32_t serial=0, payload_bytes=0, wire_bytes=0, compare_span_bytes=0, staging_copy_bytes=0;
    std::uint32_t staging_fill_bytes=0;
    std::uint16_t windows=0, bus_transactions=0;
    bool full=false, completed=false;
  };
  const TransferStats& lastTransfer() const { return transfer_; }
  const std::uint8_t* framebuffer() const { return framebuffer_; }
  std::uint32_t bufferCrc32() const;

 private:
  SPIClass& spi_;
  std::uint8_t sck_pin_;
  std::uint8_t mosi_pin_;
  std::uint8_t cs_pin_;
  std::uint8_t dc_pin_;
  std::uint8_t reset_pin_;
  bool begun_ = false;
  bool memory_valid_ = false;
  bool updating_ = false;
  alignas(4) std::uint8_t framebuffer_[BufferSize]{};
  TransferStats transfer_{};

  void hardwareReset();
  void initPanel();
  void transaction(bool is_data, const std::uint8_t* bytes,
                   std::size_t size);
  void command(std::uint8_t value);
  void data(const std::uint8_t* values, std::size_t size);
  void setFullWindow();
  void setNativeRows(std::uint8_t first, std::uint8_t last);
};
