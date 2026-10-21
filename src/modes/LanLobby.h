// 局域网对战 —— 大厅（创建/加入房间、连接握手）
#pragma once

#include <string>

namespace obnet { class NetChannel; }

// 大厅选择：0 返回菜单 / 1 创建房间 / 2 加入房间
int ShowLobby();

// 主机等待客户端接入；玩家取消返回 false
bool ShowHostWaiting(obnet::NetChannel& channel);

// 输入主机 IP；玩家取消返回 false
bool ShowJoinInput(std::string& ipOut);

// 双方 Hello 握手（协议版本 + 角色）
bool DoHandshake(obnet::NetChannel& channel, bool isHost, std::string* error);
