// 局域网对战 —— 通信协议定义（不依赖 EasyX）
//
// 传输层为 TCP，帧格式有两种（小端）：
//   1) 固定帧：8 字节
//      [0] type   [1..2] arg0   [3..4] arg1   [5..6] arg2   [7] 预留
//   2) 聊天帧（kMsgChat）：3 + N 字节
//      [0] type   [1..2] N(UTF-8 字节数)   [3..3+N) 文本
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obnet {

constexpr uint16_t kDefaultPort = 46888; // 避开 8888 等常见端口冲突
constexpr int kBoardSize = 19;
constexpr int kMsgSize = 8;
constexpr int kChatHeaderSize = 3;
constexpr int kChatMaxBytes = 180;  // 单条聊天消息的 UTF-8 字节上限（约 60 个汉字）
constexpr int kProtocolVersion = 2; // v2：新增聊天帧

enum MsgType : uint8_t {
    kMsgHello = 1,      // arg0 = 协议版本, arg1 = 角色（1 主机 / 2 客户端）
    kMsgMove,           // arg0 = x, arg1 = y
    kMsgUndoRequest,    // 请求悔棋
    kMsgUndoAccept,     // arg0 = 回滚到的历史长度
    kMsgUndoDeny,       // 拒绝悔棋
    kMsgRestartRequest, // 请求再来一局
    kMsgRestartAccept,  // 同意再来一局
    kMsgRestartDeny,    // 拒绝再来一局
    kMsgBye,            // 退出对局
    kMsgPing,
    kMsgPong,
    kMsgChat,           // 可变长聊天帧，见文件头说明
};

struct Msg {
    uint8_t type = 0;
    int16_t arg0 = 0;
    int16_t arg1 = 0;
    uint16_t arg2 = 0;
    std::string text; // 仅 kMsgChat 使用（UTF-8）
};

inline Msg MakeMsg(uint8_t type, int arg0 = 0, int arg1 = 0, int arg2 = 0) {
    Msg msg;
    msg.type = type;
    msg.arg0 = static_cast<int16_t>(arg0);
    msg.arg1 = static_cast<int16_t>(arg1);
    msg.arg2 = static_cast<uint16_t>(arg2);
    return msg;
}

inline bool IsKnownType(uint8_t type) {
    return type >= kMsgHello && type <= kMsgChat;
}

inline void EncodeMsg(const Msg& msg, uint8_t out[kMsgSize]) {
    out[0] = msg.type;
    out[1] = static_cast<uint8_t>(static_cast<uint16_t>(msg.arg0) & 0xFF);
    out[2] = static_cast<uint8_t>((static_cast<uint16_t>(msg.arg0) >> 8) & 0xFF);
    out[3] = static_cast<uint8_t>(static_cast<uint16_t>(msg.arg1) & 0xFF);
    out[4] = static_cast<uint8_t>((static_cast<uint16_t>(msg.arg1) >> 8) & 0xFF);
    out[5] = static_cast<uint8_t>(msg.arg2 & 0xFF);
    out[6] = static_cast<uint8_t>((msg.arg2 >> 8) & 0xFF);
    out[7] = 0;
}

inline bool DecodeMsg(const uint8_t in[kMsgSize], Msg& msg) {
    msg.type = in[0];
    if (!IsKnownType(msg.type) || msg.type == kMsgChat) {
        return false;
    }
    msg.arg0 = static_cast<int16_t>(static_cast<uint16_t>(in[1]) | (static_cast<uint16_t>(in[2]) << 8));
    msg.arg1 = static_cast<int16_t>(static_cast<uint16_t>(in[3]) | (static_cast<uint16_t>(in[4]) << 8));
    msg.arg2 = static_cast<uint16_t>(static_cast<uint16_t>(in[5]) | (static_cast<uint16_t>(in[6]) << 8));
    return true;
}

// 解析一条帧的结果
enum class FrameResult {
    Ok,       // 解析成功
    NeedMore, // 数据不足，等待更多字节
    Invalid,  // 协议错误
};

// 从缓冲区解析一条帧；consumed 返回消耗的字节数
inline FrameResult TryDecodeFrame(const uint8_t* data, int available, Msg& msg, int* consumed) {
    if (data == nullptr || consumed == nullptr || available < 0) {
        return FrameResult::Invalid;
    }
    if (available < 1) {
        return FrameResult::NeedMore;
    }
    const uint8_t type = data[0];
    if (!IsKnownType(type)) {
        return FrameResult::Invalid;
    }
    if (type == kMsgChat) {
        if (available < kChatHeaderSize) {
            return FrameResult::NeedMore;
        }
        const int length = static_cast<int>(data[1]) | (static_cast<int>(data[2]) << 8);
        if (length <= 0 || length > kChatMaxBytes) {
            return FrameResult::Invalid;
        }
        if (available < kChatHeaderSize + length) {
            return FrameResult::NeedMore;
        }
        msg = Msg{};
        msg.type = kMsgChat;
        msg.text.assign(reinterpret_cast<const char*>(data + kChatHeaderSize), static_cast<size_t>(length));
        *consumed = kChatHeaderSize + length;
        return FrameResult::Ok;
    }
    if (available < kMsgSize) {
        return FrameResult::NeedMore;
    }
    if (!DecodeMsg(data, msg)) {
        return FrameResult::Invalid;
    }
    msg.text.clear();
    *consumed = kMsgSize;
    return FrameResult::Ok;
}

// 编码一条聊天帧（超过 kChatMaxBytes 会被截断）
inline std::vector<uint8_t> EncodeChatFrame(const std::string& text) {
    const size_t length = text.size() > static_cast<size_t>(kChatMaxBytes)
                              ? static_cast<size_t>(kChatMaxBytes)
                              : text.size();
    std::vector<uint8_t> frame;
    frame.reserve(static_cast<size_t>(kChatHeaderSize) + length);
    frame.push_back(static_cast<uint8_t>(kMsgChat));
    frame.push_back(static_cast<uint8_t>(length & 0xFF));
    frame.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    frame.insert(frame.end(), text.begin(), text.begin() + static_cast<std::ptrdiff_t>(length));
    return frame;
}

} // namespace obnet
