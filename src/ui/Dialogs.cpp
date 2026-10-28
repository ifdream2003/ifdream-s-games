#include "ui/Dialogs.h"

#include <graphics.h>
#include <windows.h>

#include <cstring>

#include "ui/BoardView.h"
#include "ui/Display.h"

#pragma comment(lib, "MSIMG32.LIB")

// 半透明压暗整个窗口，让确认框成为视觉焦点
void DrawDimOverlay() {
    ui::DimCanvas(120);
}

// 对局中弹出模态确认框：继续游戏 / 返回菜单（menuText 可换成「返回大厅」），Esc 同「继续游戏」
ExitChoice ShowExitConfirm(const wchar_t* menuText) {
    // 记录弹框出现前的画面，Esc 取消时原样恢复
    IMAGE snapshot(ui::kCanvasW, ui::kCanvasH);
    ui::SnapshotCanvas(&snapshot);
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

    Button btnResume = { 234, 336, 150, 42, L"继续游戏", RGB(208,208,208), RGB(0,0,0) };
    Button btnMenu = { 424, 336, 150, 42, menuText != nullptr ? menuText : L"返回菜单", RGB(208,208,208), RGB(0,0,0) };
    btnResume.setFont(L"隶书"); btnResume.setFontSize(24);
    btnMenu.setFont(L"隶书"); btnMenu.setFontSize(24);
    btnResume.DrawButton();
    btnMenu.DrawButton();

    settextcolor(RGB(120, 120, 120));
    settextstyle(16, 0, L"微软雅黑");
    outtextxy(404 - textwidth(L"按 Esc 继续对局") / 2, 392, L"按 Esc 继续对局");

    ui::Present();

    // Button 的命中检测基于 MOUSEMSG，这里把 ExMessage 的坐标转成同样的结构
    auto hit = [](const Button& button, int x, int y) {
        MOUSEMSG probe;
        memset(&probe, 0, sizeof(probe));
        probe.x = static_cast<short>(x);
        probe.y = static_cast<short>(y);
        return button.IsMouseOnButton(probe);
    };

    ExMessage m;
    while (true) {
        if (!ui::PeekMessageMapped(&m, static_cast<BYTE>(EX_MOUSE | EX_KEY))) {
            ui::Pump(); // 空闲时同步窗口尺寸，拖动边框缩放能实时跟随
            Sleep(10);
            continue;
        }
        if (m.message == WM_KEYDOWN && m.vkcode == VK_ESCAPE) {
            ui::RestoreCanvas(&snapshot);
            ui::Present();
            return ExitChoice::Resume;
        }
        if (m.message != WM_LBUTTONDOWN) {
            continue;
        }
        if (hit(btnResume, m.x, m.y)) {
            ui::RestoreCanvas(&snapshot);
            ui::Present();
            return ExitChoice::Resume;
        }
        if (hit(btnMenu, m.x, m.y)) {
            return ExitChoice::BackToMenu;
        }
    }
}

// 对局结果遮罩：白底黑框 + 居中标题，避免胜负文字直接压在棋子上
void DrawResultOverlay(const wchar_t* title) {
    setfillcolor(RGB(255, 255, 255));
    solidrectangle(180, 210, 628, 410);
    setlinecolor(RGB(0, 0, 0));
    rectangle(180, 210, 628, 410);
    rectangle(181, 211, 627, 409);
    setbkmode(TRANSPARENT);
    settextcolor(RGB(180, 30, 30));
    settextstyle(48, 0, L"隶书");
    outtextxy(404 - textwidth(title) / 2, 248, title);
    settextcolor(RGB(90, 90, 90));
    settextstyle(20, 0, L"微软雅黑");
    const wchar_t* hint = L"点击右侧按钮继续";
    outtextxy(404 - textwidth(hint) / 2, 330, hint);
}

// 返回值：重新开始 / 返回菜单；对局结束后不再提供直接退出游戏的入口
EndChoice GameEndButtons() {
    MOUSEMSG m;
    // 结束界面：抹掉对局中的「悔棋」「退出」，改成面板底部的两个结束按钮
    HideUndoButton();
    if (gShowExitButton) {
        HideExitButton();
    }
    Button btnRestart(628, 492, 160, 36, L"重新开始", RGB(208, 208, 208), RGB(0, 0, 0));
    Button btnMenu(628, 540, 160, 36, L"返回菜单", RGB(208, 208, 208), RGB(0, 0, 0));
    btnRestart.setFont(L"隶书");
    btnMenu.setFont(L"隶书");
    btnRestart.setFontSize(22);
    btnMenu.setFontSize(22);
    // 先把按钮画到画布并呈现，再等待点击；否则第一次点击会落在还没有按钮的画面上
    btnRestart.DrawButton();
    btnMenu.DrawButton();
    ui::Present();
    while (true) {
        m = ui::GetMouseMsgMapped();
        if (m.uMsg != WM_LBUTTONDOWN) {
            continue;
        }
        if (btnRestart.IsMouseOnButton(m)) {
            return EndChoice::Restart;  // 重新开始
        }
        if (btnMenu.IsMouseOnButton(m)) {
            return EndChoice::BackToMenu; // 返回菜单
        }
    }
}

