#include "ui/Menu.h"

#include <graphics.h>

#include "ui/Assets.h"
#include "ui/Button.h"
#include "ui/Display.h"

// 主菜单
int MainMenu() {
    putimage(0, 0, &menu);

    //需要一个“连六棋”大字标题
    settextcolor(RGB(0, 0, 0));
    setbkmode(TRANSPARENT);
    settextstyle(80, 0, L"隶书");
    outtextxy(248, 100, L"连 六 棋");

    Button btnPVP = { 300, 250, 200, 50, L"双人对战", RGB(208,208,208),RGB(0,0,0)};
    Button btnPVC = { 300, 320, 200, 50, L"人机对战", RGB(208,208,208),RGB(0,0,0) };
    Button btnLAN = { 300, 390, 200, 50, L"局域网对战", RGB(208,208,208),RGB(0,0,0) };
    Button btnExit = { 300, 460, 200, 50, L"退出游戏",RGB(208,208,208),RGB(0,0,0) };
    btnPVP.setFont(L"隶书");
    btnPVC.setFont(L"隶书");
    btnLAN.setFont(L"隶书");
    btnExit.setFont(L"隶书");
    btnPVP. setFontSize(40);
    btnPVC. setFontSize(40);
    btnLAN.setFontSize(40);
    btnExit.setFontSize(40);

    btnPVP.DrawButton();
    btnPVC.DrawButton();
    btnLAN.DrawButton();
    btnExit.DrawButton();

    // 窗口操作提示：窗口可自由放大缩小，F11 切全屏，Ctrl +/- 等比缩放
    const wchar_t* hint = L"拖动窗口边框可放大缩小 · F11 全屏 · Ctrl + / - 缩放";
    settextcolor(RGB(72, 84, 102));
    setbkmode(TRANSPARENT);
    settextstyle(16, 0, L"微软雅黑");
    outtextxy((ui::kLogicalW - textwidth(hint)) / 2, 570, hint);

    MOUSEMSG m;
    while (true) {
        m = ui::GetMouseMsgMapped();
        //检测点击
        if (m.uMsg == WM_LBUTTONDOWN) {
            if (btnPVP.IsMouseOnButton(m)) {
                return 1;
            }
            else if (btnPVC.IsMouseOnButton(m)) {
                return 2;
            }
            else if (btnLAN.IsMouseOnButton(m)) {
                return 4;
            }
            else if (btnExit.IsMouseOnButton(m)) {
                return 3;
            }
        }
    }
}

