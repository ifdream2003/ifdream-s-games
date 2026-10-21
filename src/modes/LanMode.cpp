// 局域网对战 —— 入口：大厅 → 连接握手 → 对局会话
#include "modes/LanMode.h"

// 先引入 winsock2，再引入 graphics.h（其内部会包含 windows.h），避免 winsock1/2 冲突
#include "net/Net.h"

#include "modes/LanLobby.h"
#include "modes/LanSession.h"
#include "modes/LanUi.h"

#include "ui/Display.h"

#include <string>

using obnet::NetChannel;

void OnlineBattleWindow() {
    ui::EnsureDisplay();
    SetWindowText(GetHWnd(), L"连六棋 - 局域网对战");
    if (!LoadLanImages()) {
        ShowMessage(L"图片资源加载失败", L"请从工程目录（img 所在文件夹）运行程序");
        return;
    }

    for (;;) {
        const int choice = ShowLobby();
        if (choice == 0) {
            return;
        }

        NetChannel channel;
        std::string error;

        if (choice == 1) {
            uint16_t port = obnet::kDefaultPort;
            if (!ShowCreateInput(port)) {
                continue;
            }
            if (!channel.StartServer(port, &error)) {
                ShowMessage(L"创建房间失败", DescribeNetError(error));
                continue;
            }
            if (!ShowHostWaiting(channel, port)) {
                continue;
            }
            channel.StartReceiveThread();
            if (!DoHandshake(channel, true, &error)) {
                channel.Close();
                ShowMessage(L"连接握手失败", DescribeNetError(error));
                continue;
            }
            RunSession(channel, true);
            channel.Close();
        } else {
            std::string ip;
            uint16_t port = obnet::kDefaultPort;
            if (!ShowJoinInput(ip, port)) {
                continue;
            }
            DrawMenuBackground();
            const std::wstring endpoint = Widen(ip) + L":" + std::to_wstring(port);
            DrawTextCentered(kWindowW / 2, 240, 26, kTextRed, (L"正在连接 " + endpoint + L" …").c_str());
            DrawTextCentered(kWindowW / 2, 288, 18, kTextHint, L"最多等待 3 秒");
            ui::Present();
            Sleep(80);
            if (!channel.ConnectTo(ip, port, 3000, &error)) {
                ShowMessage(L"连接失败", DescribeNetError(error));
                continue;
            }
            channel.StartReceiveThread();
            if (!DoHandshake(channel, false, &error)) {
                channel.Close();
                ShowMessage(L"连接握手失败", DescribeNetError(error));
                continue;
            }
            RunSession(channel, false);
            channel.Close();
        }
    }
}

