#include "ST7306_OsptekLpm8.h"

#include <algorithm>

#include "st7306_osptek_lpm8_protocol.hpp"

ST7306OsptekLpm8::ST7306OsptekLpm8(
    SPIClassRP2040& spi, const std::uint8_t sck_pin,
    const std::uint8_t mosi_pin, const std::uint8_t miso_pin,
    const std::uint8_t cs_pin, const std::uint8_t dc_pin,
    const std::uint8_t reset_pin)
    : spi_(spi),
      sck_pin_(sck_pin),
      mosi_pin_(mosi_pin),
      miso_pin_(miso_pin),
      cs_pin_(cs_pin),
      dc_pin_(dc_pin),
      reset_pin_(reset_pin) {}

bool ST7306OsptekLpm8::begin() {
  pinMode(cs_pin_, OUTPUT);
  pinMode(dc_pin_, OUTPUT);
  pinMode(reset_pin_, OUTPUT);
  digitalWrite(cs_pin_, HIGH);
  digitalWrite(dc_pin_, HIGH);
  digitalWrite(reset_pin_, HIGH);

  if (!spi_.setRX(miso_pin_) || !spi_.setTX(mosi_pin_) ||
      !spi_.setSCK(sck_pin_)) {
    return false;
  }
  spi_.begin(false);

  hardwareReset();
  initPanel();
  begun_ = true;
  return true;
}

void ST7306OsptekLpm8::hardwareReset() {
  // Device-passed donor reset: high 10 ms, low 10 ms, high 10 ms.
  digitalWrite(reset_pin_, HIGH);
  delay(10);
  digitalWrite(reset_pin_, LOW);
  delay(10);
  digitalWrite(reset_pin_, HIGH);
  delay(10);
}

void ST7306OsptekLpm8::transaction(
    const bool is_data, const std::uint8_t* const bytes,
    const std::size_t size) {
  if (bytes == nullptr || size == 0U) return;
  spi_.beginTransaction(SPISettings(SpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(dc_pin_, is_data ? HIGH : LOW);
  digitalWrite(cs_pin_, LOW);
  spi_.transfer(bytes, nullptr, size);
  digitalWrite(cs_pin_, HIGH);
  spi_.endTransaction();
}

void ST7306OsptekLpm8::command(const std::uint8_t value) {
  transaction(false, &value, 1U);
}

void ST7306OsptekLpm8::data(
    const std::uint8_t* const values, const std::size_t size) {
  transaction(true, values, size);
}

void ST7306OsptekLpm8::initPanel() {
  using namespace st7306_osptek_lpm8;
  for (std::size_t i = 0; i < kInitStepCount; ++i) {
    const auto& step = kInitSteps[i];
    switch (step.kind) {
      case InitStepKind::Command:
        command(static_cast<std::uint8_t>(step.value));
        break;
      case InitStepKind::Data:
        data(step.bytes, step.size);
        break;
      case InitStepKind::DelayMs:
        delay(step.value);
        break;
    }
  }
}

void ST7306OsptekLpm8::setFullWindow() {
  constexpr std::uint8_t column_first = 0x05;
  constexpr std::uint8_t column_last = 0x36;
  constexpr std::uint8_t row_first = 0x00;
  constexpr std::uint8_t row_last = 0xC7;
  command(0x2A);
  data(&column_first, 1U);
  data(&column_last, 1U);
  command(0x2B);
  data(&row_first, 1U);
  data(&row_last, 1U);
}

void ST7306OsptekLpm8::fill(const Color color) {
  st7306_osptek_lpm8::fillFramebuffer(framebuffer_, color);
}

void ST7306OsptekLpm8::pixel(
    const int x, const int y, const Color color) {
  st7306_osptek_lpm8::writeLogicalPixel(framebuffer_, x, y, color);
}

ST7306OsptekLpm8::Color ST7306OsptekLpm8::pixelAt(
    const int x, const int y) const {
  return st7306_osptek_lpm8::readLogicalPixel(framebuffer_, x, y);
}

void ST7306OsptekLpm8::fillRect(
    const int x, const int y, const int width, const int height,
    const Color color) {
  const int left = std::max(0, x);
  const int top = std::max(0, y);
  const int right = std::min(Width, x + width);
  const int bottom = std::min(Height, y + height);
  for (int py = top; py < bottom; ++py) {
    for (int px = left; px < right; ++px) {
      pixel(px, py, color);
    }
  }
}

bool ST7306OsptekLpm8::update() {
  if (!begun_) return false;
  setFullWindow();
  command(0x2C);
  data(framebuffer_, sizeof(framebuffer_));
  return true;
}

std::uint32_t ST7306OsptekLpm8::bufferCrc32() const {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (std::size_t index = 0; index < BufferSize; ++index) {
    crc ^= framebuffer_[index];
    for (std::uint8_t bit = 0; bit < 8U; ++bit) {
      const std::uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

bool ST7306OsptekLpm8::updateNativeRows(
    const std::uint16_t first_native_row,
    const std::uint16_t last_native_row) {
  if (!begun_ || first_native_row > last_native_row ||
      last_native_row >= 200U) return false;
  constexpr std::uint8_t column_first = 0x05;
  constexpr std::uint8_t column_last = 0x36;
  const auto row_first = static_cast<std::uint8_t>(first_native_row);
  const auto row_last = static_cast<std::uint8_t>(last_native_row);
  command(0x2A);
  data(&column_first, 1U);
  data(&column_last, 1U);
  command(0x2B);
  data(&row_first, 1U);
  data(&row_last, 1U);
  command(0x2C);
  const std::uint8_t* const source =
      framebuffer_ + static_cast<std::size_t>(first_native_row) * 150U;
  const std::size_t payload_bytes =
      static_cast<std::size_t>(last_native_row - first_native_row + 1U) * 150U;
  data(source, payload_bytes);
  return true;
}
