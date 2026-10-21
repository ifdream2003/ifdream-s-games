#include "modes/LanLobby.h"

// 先引入 winsock2，再引入 graphics.h（其内部会包含 windows.h），避免 winsock1/2 冲突
#include "net/Net.h"

#include "modes/LanUi.h"

#include <graphics.h>
#include <windows.h>

#include <string>
#include <vector>

using obnet::Msg;
using obnet::NetChannel;

int ShowLobby() {
    DrawMenuBackground();
    DrawTextCentered(kWindowW / 2, 70, 60, kTextBlack, L"局域网对战", kFaceLi);
    DrawTextCentered(kWindowW / 2, 150, 22, kTextBlack, L"同一局域网内：一台创建房间，另一台输入主机 IP 加入");

    Button btnHost = MakeButton(300, 230, 200, 50, L"创建房间", 40);
    Button btnJoin = MakeButton(300, 300, 200, 50, L"加入房间", 40);
    Button btnBack = MakeButton(300, 370, 200, 50, L"返回菜单", 40);
    btnHost.DrawButton();
    btnJoin.DrawButton();
    btnBack.DrawButton();

    const std::wstring portHint = L"加入方请输入主机界面显示的 IPv4 地址，端口固定 " + std::to_wstring(obnet::kDefaultPort);
    DrawTextCentered(kWindowW / 2, 470, 18, kTextBlack, portHint.c_str());
    DrawTextCentered(kWindowW / 2, 498, 18, kTextBlack, L"主机固定执黑先手，落子规则与双人对战一致");

    ExMessage message;
    while (true) {
        getmessage(&message, EX_MOUSE);
        if (message.message != WM_LBUTTONDOWN) {
            continue;
        }
        if (HitButton(btnHost, message.x, message.y)) {
            return 1;
        }
        if (HitButton(btnJoin, message.x, message.y)) {
            return 2;
        }
        if (HitButton(btnBack, message.x, message.y)) {
            return 0;
        }
    }
}

bool ShowHostWaiting(NetChannel& channel) {
    DrawMenuBackground();
    DrawTextCentered(kWindowW / 2, 60, 50, kTextBlack, L"创建房间", kFaceLi);
    DrawTextCentered(kWindowW / 2, 138, 22, kTextBlack, L"让另一台电脑输入下面任意一个地址加入：");

    const std::vector<std::string> ips = obnet::GetLocalIPv4();
    int y = 190;
    size_t shown = 0;
    for (const std::string& ip : ips) {
        if (shown >= 3) {
            break;
        }
        DrawTextCentered(kWindowW / 2, y, 30, kTextBlack, Widen(ip).c_str());
        y += 44;
        ++shown;
    }
    const std::wstring portText = L"端口：" + std::to_wstring(obnet::kDefaultPort);
    DrawTextCentered(kWindowW / 2, y + 4, 20, kTextBlack, portText.c_str());
    DrawTextCentered(kWindowW / 2, y + 44, 24, kTextRed, L"等待对方加入…");

    Button btnCancel = MakeButton(300, y + 110, 200, 50, L"取消", 34);
    btnCancel.DrawButton();

    ExMessage message;
    while (true) {
        std::string error;
        if (channel.WaitForClient(50, &error)) {
            return true;
        }
        if (!error.empty()) {
            ShowMessage(L"等待连接失败", Widen(error));
            return false;
        }
        while (peekmessage(&message, EX_MOUSE)) {
            if (message.message == WM_LBUTTONDOWN && HitButton(btnCancel, message.x, message.y)) {
                channel.Close();
                return false;
            }
        }
        Sleep(10);
    }
}

bool ShowJoinInput(std::string& ipOut) {
    std::string ip = "127.0.0.1";
    Button btnConnect = MakeButton(300, 400, 200, 50, L"连接", 34);
    Button btnBack = MakeButton(300, 466, 200, 50, L"返回", 34);

    auto redraw = [&]() {
        DrawMenuBackground();
        DrawTextCentered(kWindowW / 2, 60, 50, kTextBlack, L"加入房间", kFaceLi);
        DrawTextCentered(kWindowW / 2, 148, 22, kTextBlack, L"输入主机的 IPv4 地址（例如 192.168.1.5）");

        setfillcolor(kWhite);
        solidrectangle(204, 200, 604, 262);
        setlinecolor(kTextBlack);
        rectangle(204, 200, 604, 262);
        rectangle(205, 201, 603, 261);

        const std::wstring wideIp = Widen(ip);
        settextstyle(30, 0, kFaceYahei);
        setbkmode(TRANSPARENT);
        settextcolor(kTextBlack);
        outtextxy(224, 216, wideIp.c_str());
        const int textWidth = textwidth(wideIp.c_str());
        outtextxy(224 + textWidth + 2, 214, L"|");

        DrawTextCentered(kWindowW / 2, 300, 18, kTextBlack, L"数字和英文句点，回车连接；Esc 返回");
        btnConnect.DrawButton();
        btnBack.DrawButton();
    };
    redraw();

    ExMessage message;
    while (true) {
        if (!peekmessage(&message, static_cast<BYTE>(EX_MOUSE | EX_KEY | EX_CHAR))) {
            Sleep(10);
            continue;
        }
        if (message.message == WM_KEYDOWN) {
            if (message.vkcode == VK_RETURN) {
                ipOut = ip;
                return true;
            }
            if (message.vkcode == VK_ESCAPE) {
                return false;
            }
            if (message.vkcode == VK_BACK && !ip.empty()) {
                ip.pop_back();
                redraw();
            }
            continue;
        }
        if (message.message == WM_CHAR) {
            const wchar_t ch = message.ch;
            if (((ch >= L'0' && ch <= L'9') || ch == L'.') && ip.size() < 15) {
                ip.push_back(static_cast<char>(ch));
                redraw();
            }
            continue;
        }
        if (message.message == WM_LBUTTONDOWN) {
            if (HitButton(btnConnect, message.x, message.y)) {
                ipOut = ip;
                return true;
            }
            if (HitButton(btnBack, message.x, message.y)) {
                return false;
            }
        }
    }
}

bool DoHandshake(NetChannel& channel, bool isHost, std::string* error) {
    channel.Send(obnet::MakeMsg(obnet::kMsgHello, obnet::kProtocolVersion, isHost ? 1 : 2));
    const DWORD deadline = GetTickCount() + 5000;
    while (GetTickCount() < deadline) {
        Msg incoming;
        while (channel.Poll(incoming)) {
            if (incoming.type == obnet::kMsgHello) {
                if (incoming.arg0 != obnet::kProtocolVersion) {
                    *error = "protocol version mismatch";
                    return false;
                }
                if (incoming.arg1 == (isHost ? 1 : 2)) {
                    *error = "role conflict";
                    return false;
                }
                return true;
            }
            if (incoming.type == obnet::kMsgBye) {
                *error = "peer left";
                return false;
            }
        }
        if (!channel.Connected()) {
            *error = channel.LastError();
            return false;
        }
        Sleep(10);
    }
    *error = "handshake timed out";
    return false;
}

