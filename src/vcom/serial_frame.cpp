#include "serial_frame.h"

#include <algorithm>

namespace mfc_tool::vcom {
namespace {

constexpr std::size_t kHeaderSize = 12u;
constexpr std::size_t kTailCrcSize = 2u;
constexpr std::size_t kMinFrameSize = kHeaderSize + kTailCrcSize;
constexpr std::size_t kMaxFramePayload = 4096u;

std::uint16_t ReadLe16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

void WriteLe16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8u) & 0xFFu);
}

} // namespace

std::uint16_t Crc16Ccitt(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8u;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<std::uint16_t>((crc << 1u) ^ 0x1021u);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1u);
            }
        }
    }
    return crc;
}

std::vector<std::uint8_t> EncodeFrame(const Frame& frame) {
    const std::size_t payload_len = frame.payload.size();
    std::vector<std::uint8_t> out(kHeaderSize + payload_len + kTailCrcSize, 0u);

    out[0] = kFrameSof0;
    out[1] = kFrameSof1;
    out[2] = kFrameVersion;
    out[3] = static_cast<std::uint8_t>(frame.type);
    WriteLe16(&out[4], frame.seq);
    out[6] = frame.cmd;
    out[7] = frame.status;
    WriteLe16(&out[8], static_cast<std::uint16_t>(payload_len));
    WriteLe16(&out[10], Crc16Ccitt(&out[2], 8u));

    if (payload_len > 0u) {
        std::copy(frame.payload.begin(), frame.payload.end(),
                  out.begin() + static_cast<std::ptrdiff_t>(kHeaderSize));
    }

    WriteLe16(&out[kHeaderSize + payload_len],
              Crc16Ccitt(payload_len == 0u ? nullptr : frame.payload.data(), payload_len));
    return out;
}

bool DecodeFrame(const std::vector<std::uint8_t>& bytes, Frame* out) {
    if (bytes.size() < kMinFrameSize || out == nullptr) {
        return false;
    }
    if (bytes[0] != kFrameSof0 || bytes[1] != kFrameSof1 || bytes[2] != kFrameVersion) {
        return false;
    }
    if (ReadLe16(&bytes[10]) != Crc16Ccitt(&bytes[2], 8u)) {
        return false;
    }

    const std::uint16_t payload_len = ReadLe16(&bytes[8]);
    if (payload_len > kMaxFramePayload) {
        return false;
    }
    const std::size_t expected_size = kHeaderSize + payload_len + kTailCrcSize;
    if (bytes.size() != expected_size) {
        return false;
    }

    const std::uint16_t payload_crc = ReadLe16(&bytes[kHeaderSize + payload_len]);
    const std::uint16_t calc_crc =
        Crc16Ccitt(payload_len == 0u ? nullptr : &bytes[kHeaderSize], payload_len);
    if (payload_crc != calc_crc) {
        return false;
    }

    out->type = static_cast<FrameType>(bytes[3]);
    out->seq = ReadLe16(&bytes[4]);
    out->cmd = bytes[6];
    out->status = bytes[7];
    out->payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderSize),
                        bytes.begin() + static_cast<std::ptrdiff_t>(kHeaderSize + payload_len));
    return true;
}

void FrameStreamParser::Push(const std::uint8_t* data, std::size_t len) {
    if (data == nullptr || len == 0u) {
        return;
    }
    buffer_.insert(buffer_.end(), data, data + static_cast<std::ptrdiff_t>(len));
}

bool FrameStreamParser::TryPop(Frame* out) {
    if (out == nullptr) {
        return false;
    }

    while (buffer_.size() >= 2u) {
        if (buffer_[0] == kFrameSof0 && buffer_[1] == kFrameSof1) {
            break;
        }
        buffer_.erase(buffer_.begin());
    }

    if (buffer_.size() < kMinFrameSize) {
        return false;
    }
    if (buffer_[2] != kFrameVersion) {
        buffer_.erase(buffer_.begin());
        return false;
    }

    const std::uint16_t payload_len = ReadLe16(&buffer_[8]);
    if (payload_len > kMaxFramePayload) {
        buffer_.erase(buffer_.begin());
        return false;
    }
    const std::size_t total = kHeaderSize + payload_len + kTailCrcSize;
    if (buffer_.size() < total) {
        return false;
    }

    std::vector<std::uint8_t> frame_bytes(buffer_.begin(),
                                          buffer_.begin() + static_cast<std::ptrdiff_t>(total));
    if (!DecodeFrame(frame_bytes, out)) {
        buffer_.erase(buffer_.begin());
        return false;
    }

    buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(total));
    return true;
}

void FrameStreamParser::Clear() {
    buffer_.clear();
}

} // namespace mfc_tool::vcom
