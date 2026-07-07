#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mfc_tool::vcom {

class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool Open(const std::wstring& com_port, std::uint32_t baudrate, std::wstring* error);
    void Close();
    [[nodiscard]] bool IsOpen() const;

    bool WriteAll(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms, std::wstring* error);
    bool ReadSome(std::vector<std::uint8_t>* out, std::size_t max_len, std::uint32_t timeout_ms, std::wstring* error);
    bool Purge(std::wstring* error);

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

} // namespace mfc_tool::vcom
