#include "serial_port.h"

#include <algorithm>
#include <sstream>

namespace mfc_tool::vcom {
namespace {

std::wstring WinErr(const wchar_t* where) {
    const DWORD code = GetLastError();
    std::wstringstream ss;
    ss << where << L" failed (GetLastError=" << code << L")";
    return ss.str();
}

} // namespace

SerialPort::SerialPort() = default;

SerialPort::~SerialPort() {
    Close();
}

bool SerialPort::Open(const std::wstring& com_port, std::uint32_t baudrate, std::wstring* error) {
    Close();

    const std::wstring path = L"\\\\.\\" + com_port;
    handle_ = CreateFileW(path.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          nullptr,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        if (error != nullptr) {
            *error = WinErr(L"CreateFileW");
        }
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        if (error != nullptr) {
            *error = WinErr(L"GetCommState");
        }
        Close();
        return false;
    }

    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
    if (!SetCommState(handle_, &dcb)) {
        if (error != nullptr) {
            *error = WinErr(L"SetCommState");
        }
        Close();
        return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 20;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 50;
    if (!SetCommTimeouts(handle_, &timeouts)) {
        if (error != nullptr) {
            *error = WinErr(L"SetCommTimeouts");
        }
        Close();
        return false;
    }

    if (!SetupComm(handle_, 4096, 4096)) {
        if (error != nullptr) {
            *error = WinErr(L"SetupComm");
        }
        Close();
        return false;
    }

    return Purge(error);
}

void SerialPort::Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

bool SerialPort::IsOpen() const {
    return handle_ != INVALID_HANDLE_VALUE;
}

bool SerialPort::WriteAll(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms, std::wstring* error) {
    if (!IsOpen()) {
        if (error != nullptr) {
            *error = L"Serial port is not open.";
        }
        return false;
    }
    if (len == 0u) {
        return true;
    }

    const DWORD started = GetTickCount();
    std::size_t offset = 0u;
    while (offset < len) {
        DWORD written = 0;
        if (!WriteFile(handle_, data + offset, static_cast<DWORD>(len - offset), &written, nullptr)) {
            if (error != nullptr) {
                *error = WinErr(L"WriteFile");
            }
            return false;
        }
        offset += written;

        if (GetTickCount() - started > timeout_ms) {
            if (error != nullptr) {
                *error = L"Write timeout.";
            }
            return false;
        }
    }
    return true;
}

bool SerialPort::ReadSome(std::vector<std::uint8_t>* out, std::size_t max_len, std::uint32_t timeout_ms, std::wstring* error) {
    if (out == nullptr || max_len == 0u) {
        return false;
    }
    out->clear();

    if (!IsOpen()) {
        if (error != nullptr) {
            *error = L"Serial port is not open.";
        }
        return false;
    }

    const DWORD started = GetTickCount();
    std::vector<std::uint8_t> tmp(max_len, 0u);
    while (true) {
        DWORD read = 0;
        if (!ReadFile(handle_, tmp.data(), static_cast<DWORD>(tmp.size()), &read, nullptr)) {
            if (error != nullptr) {
                *error = WinErr(L"ReadFile");
            }
            return false;
        }

        if (read > 0u) {
            out->assign(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(read));
            return true;
        }

        if (GetTickCount() - started >= timeout_ms) {
            if (error != nullptr) {
                *error = L"Read timeout.";
            }
            return false;
        }

        Sleep(1);
    }
}

bool SerialPort::Purge(std::wstring* error) {
    if (!IsOpen()) {
        if (error != nullptr) {
            *error = L"Serial port is not open.";
        }
        return false;
    }
    if (!PurgeComm(handle_, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR)) {
        if (error != nullptr) {
            *error = WinErr(L"PurgeComm");
        }
        return false;
    }
    return true;
}

} // namespace mfc_tool::vcom
