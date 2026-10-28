// 通信帧编解码单元测试（固定 8 字节消息 + 可变长聊天帧）
// 运行：tests\run_tests.bat
#include "net/Protocol.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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

std::vector<uint8_t> MakeChatFrame(const std::string& text) {
    std::vector<uint8_t> frame;
    frame.push_back(static_cast<uint8_t>(obnet::kMsgChat));
    frame.push_back(static_cast<uint8_t>(text.size() & 0xFF));
    frame.push_back(static_cast<uint8_t>((text.size() >> 8) & 0xFF));
    for (const char ch : text) {
        frame.push_back(static_cast<uint8_t>(ch));
    }
    return frame;
}

} // namespace

int main() {
    using namespace obnet;

    // --- 固定 8 字节消息 ---
    {
        uint8_t buffer[kMsgSize];
        EncodeMsg(MakeMsg(kMsgMove, 3, 7), buffer);
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(buffer, kMsgSize, msg, &consumed) == FrameResult::Ok, "fixed frame decodes");
        Check(consumed == kMsgSize, "fixed frame consumes 8 bytes");
        Check(msg.type == kMsgMove && msg.arg0 == 3 && msg.arg1 == 7, "fixed frame keeps args");
        Check(msg.text.empty(), "fixed frame has empty text");
    }
    {
        uint8_t buffer[5] = { static_cast<uint8_t>(kMsgMove), 0, 0, 0, 0 };
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(buffer, 5, msg, &consumed) == FrameResult::NeedMore, "truncated fixed frame needs more");
    }

    // --- 聊天帧 ---
    {
        const std::string text = "你好，联机测试"; // 21 字节 UTF-8
        const std::vector<uint8_t> frame = MakeChatFrame(text);
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(frame.data(), static_cast<int>(frame.size()), msg, &consumed) == FrameResult::Ok,
              "chat frame decodes");
        Check(consumed == static_cast<int>(frame.size()), "chat frame consumes header + payload");
        Check(msg.type == kMsgChat && msg.text == text, "chat frame keeps text");
    }
    {
        const std::string text = "abc";
        const std::vector<uint8_t> frame = MakeChatFrame(text);
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(frame.data(), 2, msg, &consumed) == FrameResult::NeedMore,
              "partial chat header needs more");
        Check(TryDecodeFrame(frame.data(), static_cast<int>(frame.size()) - 1, msg, &consumed) == FrameResult::NeedMore,
              "partial chat payload needs more");
    }
    {
        // 长度超限
        std::vector<uint8_t> frame = { static_cast<uint8_t>(kMsgChat), 0, 0 };
        const int tooLong = kChatMaxBytes + 1;
        frame[1] = static_cast<uint8_t>(tooLong & 0xFF);
        frame[2] = static_cast<uint8_t>((tooLong >> 8) & 0xFF);
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(frame.data(), static_cast<int>(frame.size()), msg, &consumed) == FrameResult::Invalid,
              "oversized chat rejected");
    }
    {
        // 空消息
        std::vector<uint8_t> frame = { static_cast<uint8_t>(kMsgChat), 0, 0 };
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(frame.data(), 3, msg, &consumed) == FrameResult::Invalid, "empty chat rejected");
    }
    {
        // 未知类型
        uint8_t buffer[1] = { 200 };
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(buffer, 1, msg, &consumed) == FrameResult::Invalid, "unknown type rejected");
    }

    // --- 连续多帧 ---
    {
        std::vector<uint8_t> stream;
        uint8_t fixedFrame[kMsgSize];
        EncodeMsg(MakeMsg(kMsgMove, 1, 2), fixedFrame);
        stream.insert(stream.end(), fixedFrame, fixedFrame + kMsgSize);
        const std::vector<uint8_t> chat = MakeChatFrame("hi");
        stream.insert(stream.end(), chat.begin(), chat.end());
        stream.push_back(static_cast<uint8_t>(kMsgPing)); // 尾帧不完整

        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(stream.data(), static_cast<int>(stream.size()), msg, &consumed) == FrameResult::Ok &&
              msg.type == kMsgMove, "stream: first frame is move");
        Check(TryDecodeFrame(stream.data() + consumed, static_cast<int>(stream.size()) - consumed, msg, &consumed) ==
              FrameResult::Ok && msg.type == kMsgChat && msg.text == "hi", "stream: second frame is chat");
        Check(TryDecodeFrame(stream.data() + 13, static_cast<int>(stream.size()) - 13, msg, &consumed) ==
              FrameResult::NeedMore, "stream: trailing partial frame needs more");
    }

    // --- 编码器与解码器往返 ---
    {
        const std::string text = "quick brown fox";
        const std::vector<uint8_t> frame = EncodeChatFrame(text);
        Check(frame.size() == 3 + text.size(), "encoder writes header and payload");
        Check(frame[0] == kMsgChat, "encoder sets chat type");
        Msg msg;
        int consumed = 0;
        Check(TryDecodeFrame(frame.data(), static_cast<int>(frame.size()), msg, &consumed) == FrameResult::Ok &&
              msg.text == text, "encoder round-trips");
    }

    if (g_failures == 0) {
        std::cout << "\nALL TESTS PASSED\n";
        return 0;
    }
    std::cout << "\n" << g_failures << " TEST(S) FAILED\n";
    return 1;
}
