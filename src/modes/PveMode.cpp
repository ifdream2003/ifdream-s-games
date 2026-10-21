#include "modes/PveMode.h"

#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include <graphics.h>
#include <windows.h>

#include "ai/Search.h"
#include "core/GameState.h"
#include "core/Rules.h"
#include "ui/BoardView.h"
#include "ui/Dialogs.h"

using namespace std;

// 人机对战窗口。返回 QuitGame 表示玩家在对局中选择直接退出游戏
LeaveMatch PVCWindow() {
    gShowExitButton = true; // 对局中显示退出按钮
    initgraph(808, 608);
    HWND hnd = GetHWnd(); // 获取窗口句柄
    SetWindowText(hnd, L"连六棋"); // 设置窗口标题
    MOUSEMSG m;
    while (true) {
        // --- 每局游戏开始时的初始化 ---
        memset(board, 0, sizeof(board));
        clearHistory();
        int result = 0;
        DrawBoard(); // 初始绘制空棋盘
        bool gameover = false;
        int playerSetTimes = 1;
        settextcolor(RGB(255, 0, 0));
        settextstyle(40, 0, L"隶书");
        outtextxy(200, 280, L"游戏开始！黑棋先行");
        //让文字显示一段时间后在刷新棋盘
        Sleep(1000);
        DrawBoard();

        // --- 游戏主循环---
        while (!gameover) {
            for (int i = 0; i < playerSetTimes; i++) {
                settextstyle(20, 0, L"微软雅黑");
                settextcolor(RGB(255, 0, 0));
                outtextxy(618, 10, L"你的回合，请落子");
                settextstyle(40, 0, L"微软雅黑");

                int col = -1, row = -1;

                //这是阻塞式等待循环，我们在这里插入按钮检测
                do {
                    m = GetMouseMsg(); // 阻塞，等待鼠标消息
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
                        // 继续游戏：重绘棋盘和回合提示，继续等待落子
                        DrawBoard();
                        settextstyle(20, 0, L"微软雅黑");
                        settextcolor(RGB(255, 0, 0));
                        outtextxy(618, 10, L"你的回合，请落子");
                        continue;
                    }
                    // --- 检测是否点击了悔棋按钮 ---
                    if (m.uMsg == WM_LBUTTONDOWN && undoButton.IsMouseOnButton(m)) {
                        if (playerSetTimes - i == 2) {
                            if (moveHistory.size() > 2) {
                                if (moveHistory.size() < 4) { // 确保有棋可悔
                                    undoMove(1);
                                    break;
                                }
                                else {
                                    undoMove(1);
                                    playerSetTimes++;
                                    break;
                                }
                            }
                            break;
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
                } while (!isValid(col, row)); // 循环条件保持不变

                if (!isValid(col, row)) {
                    // 这说明不是因为找到了有效落子点而跳出的循环
                    // 那么就是因为点击了悔棋按钮
                    // 重绘界面，因为悔棋已经修改了棋盘
                    DrawBoard();
                    continue; // 跳过本次 for 循环的剩余部分，也跳过后续的AI回合，
                    // 直接进入下一次 while (!gameover) 循环
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
                    break; // 跳出 for 循环
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    outtextxy(180, 280, L"和棋！");
                    gameover = true;
                    break; // 跳出 for 循环
                }
            }

            // 如果因为悔棋触发了 continue，那么下面的代码都不会执行
            if (gameover) break;

            settextstyle(20, 0, L"微软雅黑");
            settextcolor(RGB(255, 0, 0));
            outtextxy(618, 10, L"AI思考中...");
            
            vector<pair<int, int>> aiMoves = aiput(board);
            if (aiMoves.size() < 2) {
                cerr << "AI落子生成失败" << endl;
                gameover = true;
                break;
            }

            // AI第一步
            if (isValid(aiMoves[0].first, aiMoves[0].second)) {
                setchess(aiMoves[0].first, aiMoves[0].second, 2);
                recordMove({ aiMoves[0].first,aiMoves[0].second });
                DrawBoard();
                result = checkWin(aiMoves[0].first, aiMoves[0].second);
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
            if (gameover) break;

            // AI第二步
            if (isValid(aiMoves[1].first, aiMoves[1].second)) {
                setchess(aiMoves[1].first, aiMoves[1].second, 2);
                recordMove({ aiMoves[1].first,aiMoves[1].second });
                DrawBoard();
                result = checkWin(aiMoves[1].first, aiMoves[1].second);
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
            playerSetTimes = 2;

        }

        // 游戏结束，调用结尾按钮函数
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

