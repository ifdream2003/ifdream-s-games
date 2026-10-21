#include "ui/BoardView.h"

#include <graphics.h>

#include "core/GameState.h"
#include "ui/Assets.h"

#pragma comment(lib, "MSIMG32.LIB")

bool gShowExitButton = false; // 仅人机对战页显示并响应退出按钮
Button undoButton = { 614, 608 - 112, 188, 34, L"悔棋", RGB(255,255,255),RGB(0,0,0)};
// 人机对战页的退出按钮，放在悔棋下方，布局与局域网对战一致
Button exitButton = { 614, 608 - 66, 188, 34, L"退出", RGB(255,255,255),RGB(0,0,0)};

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
    undoButton.setFont(L"隶书");
    undoButton.setFontSize(20);
    undoButton.DrawButton();
    if (gShowExitButton) {
        exitButton.setFont(L"隶书");
        exitButton.setFontSize(20);
        exitButton.DrawButton();
    }
}
