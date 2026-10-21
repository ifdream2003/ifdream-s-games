#include <graphics.h>
#include <windows.h>

#include "modes/LanMode.h"
#include "modes/PveMode.h"
#include "modes/PvpMode.h"
#include "ui/Assets.h"
#include "ui/Menu.h"

int main() {
    LoadImages();
    initgraph(808, 608);
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    while (true) {  // 主循环，控制整个程序流程
        int mode = MainMenu();  // 返回用户选择的模式
        if (mode == 1) {
            if (PVPWindow() == LeaveMatch::QuitGame) {
                break;  // 从双人对战页直接退出游戏
            }
        }
        else if (mode == 2) {
            if (PVCWindow() == LeaveMatch::QuitGame) {
                break;  // 从人机对战页直接退出游戏
            }
        }
        else if (mode == 4) {
            OnlineBattleWindow();  // 局域网对战
        }
        else if (mode == 3) {
            break;  // 退出游戏
        }
    }

    closegraph();
    return 0;
}
