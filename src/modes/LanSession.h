// 局域网对战 —— 对局会话（落子同步、悔棋与再来一局协商）
#pragma once

namespace obnet { class NetChannel; }

// 运行一局局域网对局；返回后回到大厅
void RunSession(obnet::NetChannel& channel, bool isHost);
