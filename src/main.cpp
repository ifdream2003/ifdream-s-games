#include <graphics.h>
#include <windows.h>

#include "modes/LanMode.h"
#include "modes/PveMode.h"
#include "modes/PvpMode.h"
#include "ui/Assets.h"
#include "ui/Display.h"
#include "ui/Menu.h"

namespace {

// 游戏主流程。工程是 Windows 子系统（运行时不弹控制台窗口），
// 因此入口用 WinMain / wWinMain，而不是 main
int RunGame() {
    LoadImages();
    ui::InitDisplay();
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    while (true) {  // 主循环，控制整个程序流程
        int mode = MainMenu();  // 返回用户选择的模式
        if (mode == 1) {
            PVPWindow();  // 双人对战，结束后回到主菜单
        }
        else if (mode == 2) {
            PVCWindow();  // 人机对战，结束后回到主菜单
        }
        else if (mode == 4) {
            OnlineBattleWindow();  // 局域网对战
        }
        else if (mode == 3) {
            break;  // 退出游戏
        }
    }

    ui::ShutdownDisplay();
    return 0;
}

} // namespace

// 工程是 Windows 子系统，入口用 WinMain 即可（参数都不需要）
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return RunGame();
}
