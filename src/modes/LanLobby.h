// 局域网对战 —— 大厅（创建/加入房间、连接握手）
#pragma once

#include <cstdint>
#include <string>

namespace obnet { class NetChannel; }

// 大厅选择：0 返回菜单 / 1 创建房间 / 2 加入房间
int ShowLobby();

// 主机创建房间：输入本机监听端口；玩家取消返回 false
bool ShowCreateInput(uint16_t& portOut);

// 主机等待客户端接入；port 仅用于界面显示；玩家取消返回 false
bool ShowHostWaiting(obnet::NetChannel& channel, uint16_t port);

// 输入主机 IP 与端口；玩家取消返回 false
bool ShowJoinInput(std::string& ipOut, uint16_t& portOut);

// 双方 Hello 握手（协议版本 + 角色）
bool DoHandshake(obnet::NetChannel& channel, bool isHost, std::string* error);
