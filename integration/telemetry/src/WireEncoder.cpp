#include "draftor/telemetry/WireEncoder.h"
namespace draftor::telemetry {
namespace {
// Writes at most one bounded packet; every integer is explicit little endian.
class PacketWriter {
public:
    explicit PacketWriter(std::uint8_t* storage) noexcept : bytes(storage) {}
    void u8(std::uint8_t value) noexcept {
        if (size < kMaxPacketBytes) bytes[size++] = value;
        else valid = false;
    }
    void u16(std::uint16_t value) noexcept { u8(value & 0xff); u8(value >> 8); }
    void u32(std::uint32_t value) noexcept {
        for (unsigned shift=0; shift<32; shift+=8) u8((value >> shift) & 0xff);
    }
    void u64(std::uint64_t value) noexcept {
        for (unsigned shift=0; shift<64; shift+=8) u8((value >> shift) & 0xff);
    }
    std::uint8_t* bytes;
    std::size_t size{};
    bool valid{true};
};
}
std::uint16_t crc16_ccitt(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint16_t crc = 0xffff;
    for (std::size_t i=0; i<size; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8;
        for (unsigned bit=0; bit<8; ++bit)
            crc = static_cast<std::uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}
EncodeResult encode_frame(const QueuedEvent& item, WireScratch& scratch,
                          std::size_t output_capacity) noexcept {
    auto* output = scratch.frame;
    const auto& event = item.event;
    if (item.privacy != PrivacyLevel::Basic && item.privacy != PrivacyLevel::Detailed)
        return {EncodeStatus::InvalidEvent, 0};
    if (event.detail.text_length > kTextCapacity || event.detail.path_length > kPathCapacity)
        return {EncodeStatus::InvalidEvent, 0};
    PacketWriter writer(scratch.packet);
    writer.u8('D'); writer.u8('T'); writer.u8(kWireVersion);
    writer.u8((kSchemaRevision << 4) | static_cast<std::uint8_t>(item.privacy));
    writer.u16(static_cast<std::uint16_t>(event.type));
    writer.u16(0); // Patched with payload length once explicit serialization ends.
    const auto& envelope = event.envelope;
    writer.u64(envelope.boot_id); writer.u64(envelope.event_seq);
    writer.u64(envelope.device_monotonic_us);
    writer.u32(envelope.action_id); writer.u32(envelope.composition_id);
    writer.u32(envelope.composition_revision); writer.u32(envelope.candidate_snapshot_id);
    writer.u32(envelope.document_revision); writer.u32(envelope.layout_revision);
#include "EncodePayload.inc"
    if (item.privacy == PrivacyLevel::Detailed) {
        writer.u32(event.detail.printable_codepoint);
        writer.u64(event.detail.candidate_identity);
        writer.u8(event.detail.text_length);
        for (std::size_t i=0; i<event.detail.text_length; ++i) writer.u8(event.detail.text[i]);
        writer.u8(event.detail.path_length);
        for (std::size_t i=0; i<event.detail.path_length; ++i) writer.u8(event.detail.path[i]);
    }
    const auto payload_size = writer.size - kWireHeaderBytes;
    if (!writer.valid || payload_size > kMaxPayloadBytes) return {EncodeStatus::InvalidEvent, 0};
    writer.bytes[6] = payload_size & 0xff; writer.bytes[7] = (payload_size >> 8) & 0xff;
    writer.u16(crc16_ccitt(writer.bytes, writer.size));
    std::size_t needed = 2;
    for (std::size_t i=0; i<writer.size; ++i)
        needed += (writer.bytes[i] == kFrameStart || writer.bytes[i] == kFrameEnd || writer.bytes[i] == kFrameEscape) ? 2 : 1;
    if (output_capacity < needed) return {EncodeStatus::BufferTooSmall, 0};
    std::size_t used = 0; output[used++] = kFrameStart;
    for (std::size_t i=0; i<writer.size; ++i) {
        const auto byte = writer.bytes[i];
        if (byte == kFrameStart || byte == kFrameEnd || byte == kFrameEscape) {
            output[used++] = kFrameEscape;
            output[used++] = byte == kFrameEnd ? 0xDC : byte == kFrameEscape ? 0xDD : 0xDE;
        } else output[used++] = byte;
    }
    output[used++] = kFrameEnd;
    return {EncodeStatus::Encoded, used};
}
}
