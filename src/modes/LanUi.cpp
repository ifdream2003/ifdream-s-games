#include "modes/LanUi.h"

#include <graphics.h>
#include <windows.h>

#include <cstring>

#include "net/OnlineGame.h"

#include "ui/Assets.h"
#include "ui/Display.h"
#include "ui/Panel.h"

using obgame::Game;

// 与原版 main.cpp LoadImages/DrawBoard 使用同一套图片
IMAGE g_menuBg;
IMAGE g_boardBack;
IMAGE g_blackStone;
IMAGE g_whiteStone;
static bool g_imagesReady = false;

std::wstring Widen(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 1) {
        return std::wstring();
    }
    std::wstring wide(static_cast<size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], length);
    return wide;
}

std::string Narrow(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return std::string();
    }
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        &utf8[0], length, nullptr, nullptr);
    return utf8;
}

std::wstring DescribeNetError(const std::string& error) {
    if (error.empty()) {
        return L"未知错误";
    }
    if (error == "invalid IPv4 address") {
        return L"IP 地址格式不正确";
    }
    if (error == "connection refused") {
        return L"对方没有在监听该端口（连接被拒绝），请确认端口填写正确";
    }
    if (error == "connect timed out" || error == "connection timed out") {
        return L"连接超时，请检查 IP、端口以及两台电脑是否在同一局域网";
    }
    if (error == "network unreachable") {
        return L"网络不可达，请检查两台电脑是否在同一局域网";
    }
    if (error == "host unreachable") {
        return L"主机不可达，请确认对方仍在等待加入";
    }
    if (error == "host not found") {
        return L"找不到该主机";
    }
    if (error == "port already in use") {
        return L"该端口已被占用，请换一个端口";
    }
    if (error == "connection reset by peer") {
        return L"连接被对方重置";
    }
    if (error == "peer disconnected") {
        return L"对方已断开连接";
    }
    if (error == "peer left") {
        return L"对方已离开";
    }
    if (error == "handshake timed out") {
        return L"握手超时，请确认对方也在等待加入";
    }
    if (error == "protocol version mismatch") {
        return L"双方程序版本不一致（协议版本不同）";
    }
    if (error == "role conflict") {
        return L"双方角色冲突：两台都创建了房间";
    }
    return Widen(error);
}

void DrawTextAt(int x, int y, int size, COLORREF color, const wchar_t* text, const wchar_t* face) {
    settextstyle(size, 0, face);
    setbkmode(TRANSPARENT);
    settextcolor(color);
    outtextxy(x, y, text);
}

void DrawTextCentered(int centerX, int y, int size, COLORREF color, const wchar_t* text, const wchar_t* face) {
    settextstyle(size, 0, face);
    setbkmode(TRANSPARENT);
    settextcolor(color);
    const int width = textwidth(text);
    outtextxy(centerX - width / 2, y, text);
}

// 与原版一致的按钮工厂：灰白底、黑字、隶书
Button MakeButton(int x, int y, int w, int h, const wchar_t* text, int fontSize) {
    Button button(x, y, w, h, text, kButtonBg, kTextBlack);
    button.setFont(kFaceLi);
    button.setFontSize(fontSize);
    return button;
}

bool HitButton(const Button& button, int x, int y) {
    MOUSEMSG message;
    std::memset(&message, 0, sizeof(message));
    message.x = static_cast<short>(x);
    message.y = static_cast<short>(y);
    return button.IsMouseOnButton(message);
}

bool LoadLanImages() {
    if (g_imagesReady) {
        return true;
    }
    loadimage(&g_menuBg, AssetPath(L"menu.jpg").c_str());
    loadimage(&g_boardBack, AssetPath(L"board.png").c_str());
    loadimage(&g_blackStone, AssetPath(L"black.png").c_str());
    loadimage(&g_whiteStone, AssetPath(L"white.png").c_str());
    if (g_menuBg.getwidth() <= 0 || g_boardBack.getwidth() <= 0 ||
        g_blackStone.getwidth() <= 0 || g_whiteStone.getwidth() <= 0) {
        return false;
    }
    g_imagesReady = true;
    return true;
}

void DrawMenuBackground() {
    ui::DrawCanvasImage(&g_menuBg);
}

void DrawBoardToBuffer(const Game& game) {
    ui::DrawCanvasImage(&g_boardBack);
    for (int y = 0; y < obnet::kBoardSize; ++y) {
        for (int x = 0; x < obnet::kBoardSize; ++x) {
            const int stone = game.StoneAt(x, y);
            if (stone == 1) {
                ui::DrawCanvasImageAlpha(&g_blackStone, x * kCellSize, y * kCellSize);
            } else if (stone == 2) {
                ui::DrawCanvasImageAlpha(&g_whiteStone, x * kCellSize, y * kCellSize);
            }
        }
    }
    // 右侧状态/控制区改为深色底，保证按钮与文字醒目
    ui::DrawRightPanel();

    if (game.HistorySize() > 0) {
        const obgame::Move& last = game.History().back();
        setlinecolor(RGB(255, 0, 0));
        rectangle(last.x * kCellSize + 3, last.y * kCellSize + 3,
                  last.x * kCellSize + kCellSize - 3, last.y * kCellSize + kCellSize - 3);
    }
}

void ShowMessage(const std::wstring& title, const std::wstring& detail) {
    DrawMenuBackground();
    DrawTextCentered(kWindowW / 2, 210, 32, kTextRed, title.c_str());
    if (!detail.empty()) {
        DrawTextCentered(kWindowW / 2, 262, 20, kTextBlack, detail.c_str());
    }
    Button button = MakeButton(300, 380, 200, 50, L"返回", 34);
    button.DrawButton();
    ExMessage message;
    while (true) {
        ui::GetMessageMapped(&message, EX_MOUSE);
        if (message.message == WM_LBUTTONDOWN && HitButton(button, message.x, message.y)) {
            return;
        }
    }
}

