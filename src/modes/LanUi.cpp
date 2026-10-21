#include "modes/LanUi.h"

#include <graphics.h>
#include <windows.h>

#include <cstring>

#include "net/OnlineGame.h"

#include "ui/Assets.h"

#pragma comment(lib, "msimg32.lib")

using obgame::Game;

// 与原版 main.cpp LoadImages/DrawBoard 使用同一套图片
IMAGE g_menuBg;
IMAGE g_boardBack;
IMAGE g_blackStone;
IMAGE g_whiteStone;
IMAGE g_buffer(kWindowW, kWindowH);
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

static void putimage_alpha(int x, int y, IMAGE* image, IMAGE* dest = nullptr) {
    HDC destDc = dest ? GetImageHDC(dest) : GetImageHDC(nullptr);
    HDC srcDc = GetImageHDC(image);
    const int width = image->getwidth();
    const int height = image->getheight();
    if (width <= 0 || height <= 0) {
        return;
    }
    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(destDc, x, y, width, height, srcDc, 0, 0, width, height, blend);
}

bool LoadLanImages() {
    if (g_imagesReady) {
        return true;
    }
    loadimage(&g_menuBg, AssetPath(L"menu.png").c_str());
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
    putimage(0, 0, &g_menuBg);
}

void DrawBoardToBuffer(const Game& game) {
    putimage_alpha(0, 0, &g_boardBack, &g_buffer);
    for (int y = 0; y < obnet::kBoardSize; ++y) {
        for (int x = 0; x < obnet::kBoardSize; ++x) {
            const int stone = game.StoneAt(x, y);
            if (stone == 1) {
                putimage_alpha(x * kCellSize, y * kCellSize, &g_blackStone, &g_buffer);
            } else if (stone == 2) {
                putimage_alpha(x * kCellSize, y * kCellSize, &g_whiteStone, &g_buffer);
            }
        }
    }
    putimage(0, 0, &g_buffer);

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
        getmessage(&message, EX_MOUSE);
        if (message.message == WM_LBUTTONDOWN && HitButton(button, message.x, message.y)) {
            return;
        }
    }
}

