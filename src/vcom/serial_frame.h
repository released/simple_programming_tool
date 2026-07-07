#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mfc_tool::vcom {

constexpr std::uint8_t kFrameSof0 = 0x55;
constexpr std::uint8_t kFrameSof1 = 0xAA;
constexpr std::uint8_t kFrameVersion = 0x01;

enum class FrameType : std::uint8_t {
    Request = 0x10,
    Response = 0x20,
};

struct Frame {
    FrameType type = FrameType::Request;
    std::uint16_t seq = 0;
    std::uint8_t cmd = 0;
    std::uint8_t status = 0;
    std::vector<std::uint8_t> payload;
};

std::uint16_t Crc16Ccitt(const std::uint8_t* data, std::size_t len);
std::vector<std::uint8_t> EncodeFrame(const Frame& frame);
bool DecodeFrame(const std::vector<std::uint8_t>& bytes, Frame* out);

class FrameStreamParser {
public:
    void Push(const std::uint8_t* data, std::size_t len);
    bool TryPop(Frame* out);
    void Clear();

private:
    std::vector<std::uint8_t> buffer_;
};

} // namespace mfc_tool::vcom
