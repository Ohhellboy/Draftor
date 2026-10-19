#include "ST7306_OsptekLpm8.h"

#include <algorithm>
#include <cstring>

#include "st7306_osptek_lpm8_protocol.hpp"
#include "display_delta.hpp"

ST7306OsptekLpm8::ST7306OsptekLpm8(
    SPIClass& spi, const std::uint8_t sck_pin,
    const std::uint8_t mosi_pin, const std::uint8_t cs_pin,
    const std::uint8_t dc_pin, const std::uint8_t reset_pin)
    : spi_(spi),
      sck_pin_(sck_pin),
      mosi_pin_(mosi_pin),
      cs_pin_(cs_pin),
      dc_pin_(dc_pin),
      reset_pin_(reset_pin) {}

bool ST7306OsptekLpm8::begin() {
  if(updating_)return false;
  pinMode(cs_pin_, OUTPUT);
  pinMode(dc_pin_, OUTPUT);
  pinMode(reset_pin_, OUTPUT);
  digitalWrite(cs_pin_, HIGH);
  digitalWrite(dc_pin_, HIGH);
  digitalWrite(reset_pin_, HIGH);

  // ESP32-S3 display bus is dedicated: SCLK=12, MOSI=11, no MISO.
  spi_.begin(sck_pin_, -1, mosi_pin_, cs_pin_);
  hardwareReset();
  initPanel();
  begun_ = true;
  return true;
}

void ST7306OsptekLpm8::hardwareReset() {
  digitalWrite(reset_pin_, HIGH);
  delay(10);
  digitalWrite(reset_pin_, LOW);
  delay(10);
  digitalWrite(reset_pin_, HIGH);
  // The panel can remain in Sleep Out after an MCU panic/software reset.
  // ST7306 v0.1 section 12.1.4 (pp.103-104): wait 120ms after RSTB
  // release before Sleep Out. The later post-0x11 delay cannot satisfy this.
  delay(120);
}

void ST7306OsptekLpm8::transaction(
    const bool is_data, const std::uint8_t* const bytes,
    const std::size_t size) {
  if (bytes == nullptr || size == 0U) return;
  spi_.beginTransaction(SPISettings(SpiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(dc_pin_, is_data ? HIGH : LOW);
  digitalWrite(cs_pin_, LOW);
  if(reinterpret_cast<std::uintptr_t>(bytes)%4==0&&size%4==0){
    spi_.writeBytes(bytes,size);
  }else{
    // spiWriteNL reads ceil(n/4) uint32_t words even for short commands.
    // Preserve the donor's exact CS/DC and transmitted byte counts while
    // avoiding unaligned/out-of-range C++ reads in either logical arm.
    alignas(4) std::uint8_t padded[64];
    for(std::size_t offset=0;offset<size;){
      const auto count=std::min(sizeof(padded),size-offset);
      std::memcpy(padded,bytes+offset,count);
      const auto padding=(4-count%4)%4;
      std::memset(padded+count,0,padding);
      if(updating_){transfer_.staging_copy_bytes+=count;transfer_.staging_fill_bytes+=padding;}
      spi_.writeBytes(padded,count);offset+=count;
    }
  }
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
  setNativeRows(0x00,0xC7);
}

void ST7306OsptekLpm8::setNativeRows(const std::uint8_t row_first,
                                   const std::uint8_t row_last) {
  constexpr std::uint8_t column_first = 0x05;
  constexpr std::uint8_t column_last = 0x36;
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

void ST7306OsptekLpm8::fillRect(
    const int x, const int y, const int width, const int height,
    const Color color) {
  const int left = std::max(0, x);
  const int top = std::max(0, y);
  const int right = std::min(Width, x + width);
  const int bottom = std::min(Height, y + height);
  for (int py = top; py < bottom; ++py) {
    for (int px = left; px < right; ++px) pixel(px, py, color);
  }
}

bool ST7306OsptekLpm8::update() {
  return update(nullptr,0);
}

bool ST7306OsptekLpm8::update(const std::uint8_t* completed,std::size_t size) {
  if (!begun_||updating_||(completed&&size!=BufferSize))return false;
  updating_=true;
  const auto serial=transfer_.serial+1;
  transfer_={};transfer_.serial=serial;
#if DRAFTOR_DISPLAY_TRANSACTION
  const auto plan=st7306_osptek_lpm8::planDelta(framebuffer_,completed);
#else
  (void)completed;(void)size;
  const auto plan=st7306_osptek_lpm8::fullDeltaPlan();
#endif
  transfer_.compare_span_bytes=plan.compare_span_bytes;
  transfer_.full=plan.full;
  for(unsigned i=0;i<plan.count;++i){
    const auto& band=plan.bands[i];
    const auto offset=std::size_t(band.first_tile)*st7306_osptek_lpm8::kDeltaTileBytes;
    const auto bytes=std::size_t(band.last_tile-band.first_tile+1U)*st7306_osptek_lpm8::kDeltaTileBytes;
    setNativeRows(band.first_tile*2,band.last_tile*2+1);
    command(0x2C);
    data(framebuffer_+offset,bytes);
    ++transfer_.windows;transfer_.bus_transactions+=8;
    transfer_.payload_bytes+=bytes;
    transfer_.wire_bytes+=bytes+st7306_osptek_lpm8::kWindowWireBytes;
  }
  // writeBytes returns only after its blocking hardware FIFO writes finish;
  // it has no error/timeout acknowledgement API. This is the inherited
  // software-completion boundary, not panel settling or readback confirmation.
  transfer_.completed=true;
  updating_=false;
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
