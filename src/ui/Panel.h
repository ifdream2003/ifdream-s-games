// 右侧状态/控制面板：深色底 + 分隔线，让按钮与文字在背景图上足够醒目
#pragma once

#include <graphics.h>

#include "ui/Theme.h"

namespace ui {

constexpr int kPanelX = 608;                    // 棋盘区宽度（19 格 × 32px）
constexpr int kPanelW = WINDOW_WIDTH - kPanelX; // 200
constexpr int kPanelPadding = 20;               // 面板内容左右留白

const COLORREF kPanelBg = RGB(16, 17, 21);
const COLORREF kPanelDivider = RGB(72, 76, 86);
const COLORREF kPanelText = RGB(232, 235, 240);
const COLORREF kPanelAccent = RGB(255, 104, 104);
const COLORREF kPanelMuted = RGB(150, 156, 166);

// 铺满右侧面板（在棋盘/背景绘制之后、文字与按钮之前调用）
inline void DrawRightPanel() {
    setfillcolor(kPanelBg);
    solidrectangle(kPanelX, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1);
    setlinecolor(kPanelDivider);
    line(kPanelX, 0, kPanelX, WINDOW_HEIGHT - 1);
}

// 面板内一行文字：统一字体、透明背景，避免在深色底上出现白底字块
inline void DrawPanelText(int x, int y, int size, COLORREF color, const wchar_t* text) {
    settextstyle(size, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);
    settextcolor(color);
    outtextxy(x, y, text);
}

} // namespace ui
