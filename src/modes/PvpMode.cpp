#include "modes/PvpMode.h"

#include <cstring>

#include <graphics.h>
#include <windows.h>

#include "core/GameState.h"
#include "core/Rules.h"
#include "ui/BoardView.h"
#include "ui/Dialogs.h"

//人人对战窗口
LeaveMatch PVPWindow() {
    gShowExitButton = true; // 对局中显示退出按钮
    initgraph(808, 608);
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    MOUSEMSG m;

    while (true) {
        memset(board, 0, sizeof(board));
        clearHistory();

        int result = 0;
        DrawBoard();
        bool gameover = false;
        int playerSetTimes = 1;
        int currentPlayer = 1; // 1 黑棋，2 白棋
        settextcolor(RGB(255, 0, 0));
        settextstyle(40, 0, L"隶书");
        outtextxy(200, 280, L"游戏开始！黑棋先行");
        Sleep(1000);
        DrawBoard();

        while (!gameover) {
            bool undoFlag = false;

            // 黑棋回合
            if (currentPlayer == 1) {
                for (int i = 0; i < playerSetTimes  && !undoFlag; i++) {
                    settextstyle(20, 0, L"微软雅黑");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(618, 10, L"黑棋回合，请落子");
                    int col = -1, row = -1;
                    do {
                        m = GetMouseMsg();

                        // --- 检测是否点击了退出按钮 ---
                        if (m.uMsg == WM_LBUTTONDOWN && exitButton.IsMouseOnButton(m)) {
                            const ExitChoice choice = ShowExitConfirm();
                            if (choice == ExitChoice::BackToMenu) {
                                gShowExitButton = false;
                                return LeaveMatch::BackToMenu;
                            }
                            if (choice == ExitChoice::QuitGame) {
                                gShowExitButton = false;
                                return LeaveMatch::QuitGame;
                            }
                            // 继续游戏：重绘棋盘与回合提示，继续等待落子
                            DrawBoard();
                            settextstyle(20, 0, L"微软雅黑");
                            settextcolor(RGB(255, 0, 0));
                            outtextxy(618, 10, L"黑棋回合，请落子");
                            continue;
                        }

                        if (m.uMsg == WM_LBUTTONDOWN && undoButton.IsMouseOnButton(m)) {
                            if (playerSetTimes - i == 2) {
                                if (canUndo()) {
                                    undoMove(2);
                                    currentPlayer = 2;
                                    undoFlag = true;
                                    break;
                                }
                            }
                            else {
                                settextstyle(20, 0, L"微软雅黑");
                                settextcolor(RGB(255, 0, 0));
                                outtextxy(620, 45, L"完成本回合后方可悔棋");
                            }
                        }

                        if (m.uMsg == WM_LBUTTONDOWN) {
                            col = m.x / 32;
                            row = m.y / 32;
                        }
                    } while (!isValid(col, row));

                    if (!isValid(col, row)) {
                        DrawBoard();
                        continue;
                    }

                    setchess(col, row, 1);
                    recordMove({ col, row });
                    DrawBoard();

                    result = checkWin(col, row);
                    if (result == 1) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"黑棋胜利！");
                        gameover = true;
                        break;
                    }
                    else if (result == 3) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"和棋！");
                        gameover = true;
                        break;
                    }
                }
                if (gameover || undoFlag) continue;

                currentPlayer = 2; // 切换到白棋
                playerSetTimes = 2;
            }

            // 白棋回合
            if (currentPlayer == 2) {
                for (int i = 0; i < playerSetTimes  && !undoFlag; i++) {
                    settextstyle(20, 0, L"微软雅黑");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(618, 10, L"白棋回合，请落子");

                    int col = -1, row = -1;
                    do {
                        m = GetMouseMsg();
                        // --- 检测是否点击了退出按钮 ---
                        if (m.uMsg == WM_LBUTTONDOWN && exitButton.IsMouseOnButton(m)) {
                            const ExitChoice choice = ShowExitConfirm();
                            if (choice == ExitChoice::BackToMenu) {
                                gShowExitButton = false;
                                return LeaveMatch::BackToMenu;
                            }
                            if (choice == ExitChoice::QuitGame) {
                                gShowExitButton = false;
                                return LeaveMatch::QuitGame;
                            }
                            // 继续游戏：重绘棋盘与回合提示，继续等待落子
                            DrawBoard();
                            settextstyle(20, 0, L"微软雅黑");
                            settextcolor(RGB(255, 0, 0));
                            outtextxy(618, 10, L"白棋回合，请落子");
                            continue;
                        }
                        if (m.uMsg == WM_LBUTTONDOWN && undoButton.IsMouseOnButton(m)) {
                            if (playerSetTimes - i == 2) {
                                if (moveHistory.size() == 1) {
                                    playerSetTimes = 1;
                                    currentPlayer = 1;
                                    undoMove(2);
                                    undoFlag = true;
                                    break;
                                }
                                else if (canUndo()) {
                                    undoMove(2);
                                    currentPlayer = 1;
                                    undoFlag = true;
                                    break;
                                }
                            }
                            else {
                                // 玩家未完成回合，提示并忽略点击
                                settextstyle(20, 0, L"微软雅黑");
                                settextcolor(RGB(255, 0, 0));
                                outtextxy(620, 45, L"完成本回合后方可悔棋");
                            }
                        }

                        if (m.uMsg == WM_LBUTTONDOWN) {
                            col = m.x / 32;
                            row = m.y / 32;
                        }
                    } while (!isValid(col, row));

                    if (!isValid(col, row)) {
                        DrawBoard();
                        continue;
                    }

                    setchess(col, row, 2);
                    recordMove({ col, row });
                    DrawBoard();

                    result = checkWin(col, row);
                    if (result == 2) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"白棋胜利！");
                        gameover = true;
                        break;
                    }
                    else if (result == 3) {
                        settextstyle(40, 0, L"隶书");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(180, 280, L"和棋！");
                        gameover = true;
                        break;
                    }
                }
                if (gameover || undoFlag) continue;

                currentPlayer = 1; // 切换到黑棋
                playerSetTimes = 2;
            }
        }
        const EndChoice endChoice = GameEndButtons();
        if (endChoice == EndChoice::BackToMenu) {
            gShowExitButton = false;
            return LeaveMatch::BackToMenu;  // 用户选择退回主菜单
        }
        if (endChoice == EndChoice::QuitGame) {
            gShowExitButton = false;
            return LeaveMatch::QuitGame;    // 用户选择直接退出游戏
        }
        // 选择重新开始时继续下一局
    }

    return LeaveMatch::BackToMenu; // 兜底：正常流程不会走到这里
}

