// 局域网对战 —— 连接参数校验（不依赖 EasyX / Winsock，可独立测试）
#pragma once

#include <cstdint>
#include <string>

namespace obnet {

// 校验点分十进制 IPv4 地址：四段，每段 0-255
inline bool IsValidIPv4(const std::string& text) {
    int dots = 0;
    int digits = 0;
    int value = 0;
    for (const char ch : text) {
        if (ch == '.') {
            if (digits == 0 || dots == 3) {
                return false;
            }
            ++dots;
            digits = 0;
            value = 0;
            continue;
        }
        if (ch < '0' || ch > '9') {
            return false;
        }
        if (digits == 3) {
            return false;
        }
        value = value * 10 + (ch - '0');
        if (value > 255) {
            return false;
        }
        ++digits;
    }
    return dots == 3 && digits > 0;
}

// 解析端口字符串：1-65535
inline bool TryParsePort(const std::string& text, uint16_t& port) {
    if (text.empty() || text.size() > 5) {
        return false;
    }
    unsigned value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(ch - '0');
        if (value > 65535) {
            return false;
        }
    }
    if (value == 0) {
        return false;
    }
    port = static_cast<uint16_t>(value);
    return true;
}

} // namespace obnet
