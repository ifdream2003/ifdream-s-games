// 局域网连接参数校验的单元测试（独立 exe，不依赖 EasyX）
// 运行：tests\run_tests.bat
#include "net/Endpoint.h"

#include <cstdint>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "PASS " << name << "\n";
    } else {
        std::cout << "FAIL " << name << "\n";
        ++g_failures;
    }
}

} // namespace

int main() {
    using obnet::IsValidIPv4;
    using obnet::TryParsePort;

    // --- IPv4 合法 ---
    Check(IsValidIPv4("192.168.1.5"), "ip accepts LAN address");
    Check(IsValidIPv4("127.0.0.1"), "ip accepts loopback");
    Check(IsValidIPv4("0.0.0.0"), "ip accepts zero address");
    Check(IsValidIPv4("255.255.255.255"), "ip accepts max address");

    // --- IPv4 非法 ---
    Check(!IsValidIPv4(""), "ip rejects empty");
    Check(!IsValidIPv4("192.168.1"), "ip rejects three segments");
    Check(!IsValidIPv4("192.168.1.5.7"), "ip rejects five segments");
    Check(!IsValidIPv4("192.168.1.256"), "ip rejects segment above 255");
    Check(!IsValidIPv4("192.168..5"), "ip rejects empty segment");
    Check(!IsValidIPv4(".192.168.1"), "ip rejects leading dot");
    Check(!IsValidIPv4("192.168.1."), "ip rejects trailing dot");
    Check(!IsValidIPv4("abc.def.1.2"), "ip rejects letters");
    Check(!IsValidIPv4("192.168.1.5 "), "ip rejects trailing space");
    Check(!IsValidIPv4("192.168.1.1000"), "ip rejects four-digit segment");

    // --- 端口合法 ---
    uint16_t port = 0;
    Check(TryParsePort("46888", port) && port == 46888, "port parses 46888");
    Check(TryParsePort("1", port) && port == 1, "port parses minimum");
    Check(TryParsePort("65535", port) && port == 65535, "port parses maximum");

    // --- 端口非法 ---
    Check(!TryParsePort("", port), "port rejects empty");
    Check(!TryParsePort("0", port), "port rejects zero");
    Check(!TryParsePort("65536", port), "port rejects above maximum");
    Check(!TryParsePort("12a", port), "port rejects letters");
    Check(!TryParsePort(" 80", port), "port rejects leading space");
    Check(!TryParsePort("-1", port), "port rejects negative");
    Check(!TryParsePort("123456", port), "port rejects six digits");

    if (g_failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
        return 0;
    }
    std::cout << "\n" << g_failures << " TEST(S) FAILED\n";
    return 1;
}
