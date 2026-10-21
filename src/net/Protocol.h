// 局域网对战 —— 通信协议定义（不依赖 EasyX）
//
// 传输层为 TCP，每条消息固定 8 字节（小端）：
//   [0]      type    消息类型
//   [1][2]   arg0    int16  通用参数（落子 x / 目标历史长度）
//   [3][4]   arg1    int16  通用参数（落子 y / 角色）
//   [5][6]   arg2    uint16 预留
//   [7]      预留
#pragma once

#include <cstdint>

namespace obnet {

constexpr uint16_t kDefaultPort = 46888; // 避开 8888 等常见端口冲突
constexpr int kBoardSize = 19;
constexpr int kMsgSize = 8;
constexpr int kProtocolVersion = 1;

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
};

struct Msg {
    uint8_t type = 0;
    int16_t arg0 = 0;
    int16_t arg1 = 0;
    uint16_t arg2 = 0;
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
    return type >= kMsgHello && type <= kMsgPong;
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
    if (!IsKnownType(msg.type)) {
        return false;
    }
    msg.arg0 = static_cast<int16_t>(static_cast<uint16_t>(in[1]) | (static_cast<uint16_t>(in[2]) << 8));
    msg.arg1 = static_cast<int16_t>(static_cast<uint16_t>(in[3]) | (static_cast<uint16_t>(in[4]) << 8));
    msg.arg2 = static_cast<uint16_t>(static_cast<uint16_t>(in[5]) | (static_cast<uint16_t>(in[6]) << 8));
    return true;
}

} // namespace obnet
