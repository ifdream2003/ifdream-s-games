#include "modes/LanLobby.h"

// 先引入 winsock2，再引入 graphics.h（其内部会包含 windows.h），避免 winsock1/2 冲突
#include "net/Net.h"

#include "modes/LanUi.h"

#include "ui/Display.h"

#include <graphics.h>
#include <windows.h>

#include <string>
#include <vector>

#include "net/Endpoint.h"

using obnet::Msg;
using obnet::NetChannel;

namespace {

constexpr int kFieldX = 204;
constexpr int kFieldW = 400;
constexpr int kFieldH = 52;
constexpr int kFieldFontSize = 30;
constexpr int kFieldPadX = 18;

// 单行输入框：只保存字符，绘制与命中检测都在本文件内完成
struct InputField {
    int x = kFieldX;
    int y = 0;
    std::string text;
    size_t maxLength = 0;
    bool digitsOnly = false;
};

bool HitField(const InputField& field, int mx, int my) {
    return mx >= field.x && mx <= field.x + kFieldW &&
           my >= field.y && my <= field.y + kFieldH;
}

void DrawField(const InputField& field, bool focused) {
    setfillcolor(kWhite);
    solidrectangle(field.x, field.y, field.x + kFieldW, field.y + kFieldH);

    setlinecolor(focused ? kFocusBorder : kTextBlack);
    rectangle(field.x, field.y, field.x + kFieldW, field.y + kFieldH);
    if (focused) {
        rectangle(field.x + 1, field.y + 1, field.x + kFieldW - 1, field.y + kFieldH - 1);
    }

    const std::wstring wide = Widen(field.text);
    const int textY = field.y + (kFieldH - kFieldFontSize) / 2 - 2;
    DrawTextAt(field.x + kFieldPadX, textY, kFieldFontSize, kTextBlack, wide.c_str());

    if (focused) {
        settextstyle(kFieldFontSize, 0, kFaceYahei);
        const int caretX = field.x + kFieldPadX + (wide.empty() ? 0 : textwidth(wide.c_str())) + 2;
        DrawTextAt(caretX, textY, kFieldFontSize, kTextBlack, L"|");
    }
}

void DrawFieldLabel(const InputField& field, const wchar_t* text) {
    DrawTextAt(field.x, field.y - 28, 20, kTextBlack, text);
}

// 追加一个字符；端口框只接受数字，IP 框额外接受点
bool AcceptChar(InputField& field, wchar_t ch) {
    if (field.text.size() >= field.maxLength) {
        return false;
    }
    if (ch >= L'0' && ch <= L'9') {
        field.text.push_back(static_cast<char>(ch));
        return true;
    }
    if (!field.digitsOnly && ch == L'.') {
        field.text.push_back('.');
        return true;
    }
    return false;
}

bool Backspace(InputField& field) {
    if (field.text.empty()) {
        return false;
    }
    field.text.pop_back();
    return true;
}

} // namespace

int ShowLobby() {
    DrawMenuBackground();
    DrawTextCentered(kWindowW / 2, 70, 60, kTextBlack, L"局域网对战", kFaceLi);
    DrawTextCentered(kWindowW / 2, 150, 22, kTextBlack, L"同一局域网内：一台创建房间，另一台用 IP 和端口加入");

    Button btnHost = MakeButton(300, 230, 200, 50, L"创建房间", 40);
    Button btnJoin = MakeButton(300, 300, 200, 50, L"加入房间", 40);
    Button btnBack = MakeButton(300, 370, 200, 50, L"返回菜单", 40);
    btnHost.DrawButton();
    btnJoin.DrawButton();
    btnBack.DrawButton();
        ui::Present();

    DrawTextCentered(kWindowW / 2, 470, 18, kTextBlack, L"创建房间时可自定义端口；加入方填写主机界面显示的 IP 与端口");
    DrawTextCentered(kWindowW / 2, 498, 18, kTextBlack, L"主机固定执黑先手，落子规则与双人对战一致");

    ExMessage message;
    while (true) {
        ui::GetMessageMapped(&message, EX_MOUSE);
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

bool ShowCreateInput(uint16_t& portOut) {
    InputField portField;
    portField.y = 232;
    portField.text = std::to_string(obnet::kDefaultPort);
    portField.maxLength = 5;
    portField.digitsOnly = true;
    std::wstring error;

    Button btnStart = MakeButton(300, 388, 200, 50, L"开始等待", 32);
    Button btnBack = MakeButton(300, 454, 200, 50, L"返回", 34);

    auto redraw = [&]() {
        DrawMenuBackground();
        DrawTextCentered(kWindowW / 2, 54, 46, kTextBlack, L"创建房间", kFaceLi);
        DrawTextCentered(kWindowW / 2, 122, 20, kTextBlack, L"设置本机监听的端口，对方加入时填写相同的端口");
        DrawFieldLabel(portField, L"监听端口");
        DrawField(portField, true);
        if (!error.empty()) {
            DrawTextAt(kFieldX, 312, 20, kTextRed, error.c_str());
        } else {
            DrawTextAt(kFieldX, 312, 18, kTextHint, L"回车开始等待 · Esc 返回");
        }
        btnStart.DrawButton();
        btnBack.DrawButton();
        ui::Present();
    };
    redraw();

    auto submit = [&]() -> bool {
        uint16_t port = 0;
        if (!obnet::TryParsePort(portField.text, port)) {
            error = portField.text.empty() ? L"请输入端口（默认 46888）" : L"端口需为 1-65535 之间的整数";
            redraw();
            return false;
        }
        portOut = port;
        return true;
    };

    ExMessage message;
    while (true) {
        if (!ui::PeekMessageMapped(&message, static_cast<BYTE>(EX_MOUSE | EX_KEY | EX_CHAR))) {
            ui::Pump(); // 空闲时同步窗口尺寸，拖动边框缩放能实时跟随
            Sleep(10);
            continue;
        }
        if (message.message == WM_KEYDOWN) {
            if (message.vkcode == VK_RETURN) {
                if (submit()) {
                    return true;
                }
                continue;
            }
            if (message.vkcode == VK_ESCAPE) {
                return false;
            }
            if (message.vkcode == VK_BACK && Backspace(portField)) {
                error.clear();
                redraw();
            }
            continue;
        }
        if (message.message == WM_CHAR) {
            if (AcceptChar(portField, message.ch)) {
                error.clear();
                redraw();
            }
            continue;
        }
        if (message.message == WM_LBUTTONDOWN) {
            if (HitButton(btnStart, message.x, message.y)) {
                if (submit()) {
                    return true;
                }
                continue;
            }
            if (HitButton(btnBack, message.x, message.y)) {
                return false;
            }
        }
    }
}

bool ShowHostWaiting(NetChannel& channel, uint16_t port) {
    DrawMenuBackground();
    DrawTextCentered(kWindowW / 2, 60, 50, kTextBlack, L"创建房间", kFaceLi);
    DrawTextCentered(kWindowW / 2, 138, 22, kTextBlack, L"让另一台电脑用下面的 IP 和端口加入：");

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
    const std::wstring portText = L"端口：" + std::to_wstring(port);
    DrawTextCentered(kWindowW / 2, y + 4, 22, kTextBlack, portText.c_str());
    DrawTextCentered(kWindowW / 2, y + 48, 24, kTextRed, L"等待对方加入…");

    Button btnCancel = MakeButton(300, y + 116, 200, 50, L"取消", 34);
    btnCancel.DrawButton();
    ui::Present();

    ExMessage message;
    while (true) {
        std::string error;
        if (channel.WaitForClient(50, &error)) {
            return true;
        }
        if (!error.empty()) {
            ShowMessage(L"等待连接失败", DescribeNetError(error));
            return false;
        }
        while (ui::PeekMessageMapped(&message, EX_MOUSE)) {
            if (message.message == WM_LBUTTONDOWN && HitButton(btnCancel, message.x, message.y)) {
                channel.Close();
                return false;
            }
        }
        ui::Pump(); // 空闲时同步窗口尺寸
        Sleep(10);
    }
}

bool ShowJoinInput(std::string& ipOut, uint16_t& portOut) {
    InputField ipField;
    ipField.y = 214;
    ipField.text = "127.0.0.1";
    ipField.maxLength = 15;

    InputField portField;
    portField.y = 322;
    portField.text = std::to_string(obnet::kDefaultPort);
    portField.maxLength = 5;
    portField.digitsOnly = true;

    int focus = 0; // 0 = IP，1 = 端口
    std::wstring error;

    Button btnConnect = MakeButton(300, 452, 200, 50, L"连接", 34);
    Button btnBack = MakeButton(300, 518, 200, 50, L"返回", 34);

    auto redraw = [&]() {
        DrawMenuBackground();
        DrawTextCentered(kWindowW / 2, 50, 46, kTextBlack, L"加入房间", kFaceLi);
        DrawTextCentered(kWindowW / 2, 118, 20, kTextBlack, L"填写主机「创建房间」界面上显示的 IP 与端口");
        DrawFieldLabel(ipField, L"主机 IP");
        DrawField(ipField, focus == 0);
        DrawFieldLabel(portField, L"端口");
        DrawField(portField, focus == 1);
        if (!error.empty()) {
            DrawTextAt(kFieldX, 396, 20, kTextRed, error.c_str());
        } else {
            DrawTextAt(kFieldX, 396, 18, kTextHint, L"回车连接 · Tab 切换输入框 · Esc 返回");
        }
        btnConnect.DrawButton();
        btnBack.DrawButton();
        ui::Present();
    };
    redraw();

    auto submit = [&]() -> bool {
        if (!obnet::IsValidIPv4(ipField.text)) {
            error = ipField.text.empty() ? L"请输入主机 IP 地址" : L"IP 地址格式不正确，应形如 192.168.1.5";
            focus = 0;
            redraw();
            return false;
        }
        uint16_t port = 0;
        if (!obnet::TryParsePort(portField.text, port)) {
            error = portField.text.empty() ? L"请输入端口（默认 46888）" : L"端口需为 1-65535 之间的整数";
            focus = 1;
            redraw();
            return false;
        }
        ipOut = ipField.text;
        portOut = port;
        return true;
    };

    ExMessage message;
    while (true) {
        if (!ui::PeekMessageMapped(&message, static_cast<BYTE>(EX_MOUSE | EX_KEY | EX_CHAR))) {
            ui::Pump(); // 空闲时同步窗口尺寸
            Sleep(10);
            continue;
        }
        if (message.message == WM_KEYDOWN) {
            if (message.vkcode == VK_RETURN) {
                if (submit()) {
                    return true;
                }
                continue;
            }
            if (message.vkcode == VK_ESCAPE) {
                return false;
            }
            if (message.vkcode == VK_TAB) {
                focus = 1 - focus;
                redraw();
                continue;
            }
            if (message.vkcode == VK_BACK) {
                InputField& active = focus == 0 ? ipField : portField;
                if (Backspace(active)) {
                    error.clear();
                    redraw();
                }
            }
            continue;
        }
        if (message.message == WM_CHAR) {
            InputField& active = focus == 0 ? ipField : portField;
            if (AcceptChar(active, message.ch)) {
                error.clear();
                redraw();
            }
            continue;
        }
        if (message.message == WM_LBUTTONDOWN) {
            if (HitButton(btnConnect, message.x, message.y)) {
                if (submit()) {
                    return true;
                }
                continue;
            }
            if (HitButton(btnBack, message.x, message.y)) {
                return false;
            }
            if (HitField(ipField, message.x, message.y)) {
                focus = 0;
                redraw();
            } else if (HitField(portField, message.x, message.y)) {
                focus = 1;
                redraw();
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
