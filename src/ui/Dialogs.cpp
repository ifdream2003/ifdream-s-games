#include "ui/Dialogs.h"

#include <graphics.h>
#include <windows.h>

#include "ui/BoardView.h"

#pragma comment(lib, "MSIMG32.LIB")

// 半透明压暗整个窗口，让确认框成为视觉焦点
void DrawDimOverlay() {
    IMAGE dim(808, 608);
    SetWorkingImage(&dim);
    setfillcolor(RGB(0, 0, 0));
    solidrectangle(0, 0, 808, 608);
    SetWorkingImage(NULL);

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 120;
    blend.AlphaFormat = 0;
    AlphaBlend(GetImageHDC(NULL), 0, 0, 808, 608,
        GetImageHDC(&dim), 0, 0, 808, 608, blend);
}

// 对局中弹出模态确认框：继续游戏 / 返回主菜单 / 退出游戏
ExitChoice ShowExitConfirm() {
    DrawDimOverlay();

    // 确认面板
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(164, 180, 644, 440);
    setlinecolor(RGB(0, 0, 0));
    rectangle(164, 180, 644, 440);
    rectangle(165, 181, 643, 439);

    setbkmode(TRANSPARENT);
    settextcolor(RGB(0, 0, 0));
    settextstyle(36, 0, L"隶书");
    outtextxy(404 - textwidth(L"退出对局？") / 2, 206, L"退出对局？");

    settextcolor(RGB(90, 90, 90));
    settextstyle(20, 0, L"微软雅黑");
    outtextxy(404 - textwidth(L"当前棋局不会保存") / 2, 262, L"当前棋局不会保存");

    Button btnResume = { 194, 360, 130, 38, L"继续游戏", RGB(208,208,208), RGB(0,0,0) };
    Button btnMenu = { 339, 360, 130, 38, L"返回菜单", RGB(208,208,208), RGB(0,0,0) };
    Button btnQuit = { 484, 360, 130, 38, L"退出游戏", RGB(208,208,208), RGB(0,0,0) };
    btnResume.setFont(L"隶书"); btnResume.setFontSize(22);
    btnMenu.setFont(L"隶书"); btnMenu.setFontSize(22);
    btnQuit.setFont(L"隶书"); btnQuit.setFontSize(22);
    btnResume.DrawButton();
    btnMenu.DrawButton();
    btnQuit.DrawButton();

    MOUSEMSG m;
    while (true) {
        m = GetMouseMsg();
        if (m.uMsg != WM_LBUTTONDOWN) {
            continue;
        }
        if (btnResume.IsMouseOnButton(m)) {
            return ExitChoice::Resume;
        }
        if (btnMenu.IsMouseOnButton(m)) {
            return ExitChoice::BackToMenu;
        }
        if (btnQuit.IsMouseOnButton(m)) {
            return ExitChoice::QuitGame;
        }
    }
}

// 返回值：重新开始 / 返回菜单 / 退出游戏
EndChoice GameEndButtons() {
    MOUSEMSG m;
    Button btnRestart(618, 350, 150, 40, L"重新开始", RGB(208, 208, 208), RGB(0, 0, 0));
    Button btnMenu(618, 400, 150, 40, L"返回菜单", RGB(208, 208, 208), RGB(0, 0, 0));
    btnRestart.setFont(L"隶书");
    btnMenu.setFont(L"隶书");
    btnRestart.setFontSize(30);
    btnMenu.setFontSize(30);
    while (true) {
        m = GetMouseMsg();
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (btnRestart.IsMouseOnButton(m)) {
                return EndChoice::Restart;  // 重新开始
            }
            else if (btnMenu.IsMouseOnButton(m)) {
                return EndChoice::BackToMenu; // 返回菜单
            }
            else if (gShowExitButton && exitButton.IsMouseOnButton(m)) {
                return EndChoice::QuitGame; // 对局已结束，直接退出游戏
            }
        }
        BeginBatchDraw();
        btnRestart.DrawButton();
        btnMenu.DrawButton();
        EndBatchDraw();
    }
}

