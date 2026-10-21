#include "ui/BoardView.h"

#include <graphics.h>

#include "core/GameState.h"
#include "ui/Assets.h"
#include "ui/Panel.h"

#pragma comment(lib, "MSIMG32.LIB")

bool gShowExitButton = false; // 仅人机对战页显示并响应退出按钮
// 右侧控制区按钮：与局域网对战保持一致（面板宽 200px，按钮 160 宽居中，左右各留 20px）
constexpr int kPanelButtonX = 628;
constexpr int kPanelButtonW = 160;
constexpr int kPanelButtonH = 36;
constexpr int kUndoButtonY = 492;
constexpr int kExitButtonY = 540;

Button undoButton = { kPanelButtonX, kUndoButtonY, kPanelButtonW, kPanelButtonH, L"悔棋", RGB(255,255,255),RGB(0,0,0)};
// 人机对战页的退出按钮，放在悔棋下方
Button exitButton = { kPanelButtonX, kExitButtonY, kPanelButtonW, kPanelButtonH, L"退出", RGB(255,255,255),RGB(0,0,0)};

void HideExitButton() {
    setfillcolor(ui::kPanelBg);
    solidrectangle(kPanelButtonX, kExitButtonY,
                   kPanelButtonX + kPanelButtonW, kExitButtonY + kPanelButtonH);
}

void HideUndoButton() {
    setfillcolor(ui::kPanelBg);
    solidrectangle(kPanelButtonX, kUndoButtonY,
                   kPanelButtonX + kPanelButtonW, kUndoButtonY + kPanelButtonH);
}

inline void putimage_alpha(int x, int y, IMAGE* img, IMAGE* dest = NULL) {
    HDC hdcDest = dest ? GetImageHDC(dest) : GetImageHDC(NULL);
    HDC hdcSrc = GetImageHDC(img);

    int w = img->getwidth();
    int h = img->getheight();

    AlphaBlend(
        hdcDest, x, y, w, h,
        hdcSrc, 0, 0, w, h,
        { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA }
    );
}

//打印棋盘
void DrawBoard() {
    // 绘制背景到缓冲区
    putimage_alpha(0, 0, &back, &buffer);

    // 绘制棋子到缓冲区
    for (int i = 0; i < 19; i++) {
        for (int j = 0; j < 19; j++) {
            int x = j * 32;
            int y = i * 32;
            if (board[i][j] == 1) {
                putimage_alpha(x, y, &black, &buffer);
            }
            else if (board[i][j] == 2) {
                putimage_alpha(x, y, &white, &buffer);
            }
        }
    }
    // 将缓冲区绘制到屏幕
    putimage(0, 0, &buffer);
    // 右侧状态/控制区改为深色底，保证按钮与文字醒目
    ui::DrawRightPanel();
    undoButton.setFont(L"隶书");
    undoButton.setFontSize(20);
    undoButton.DrawButton();
    if (gShowExitButton) {
        exitButton.setFont(L"隶书");
        exitButton.setFontSize(20);
        exitButton.DrawButton();
    }
}
