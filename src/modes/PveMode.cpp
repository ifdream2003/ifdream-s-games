#include "modes/PveMode.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <graphics.h>
#include <windows.h>

#include "ai/Search.h"
#include "core/GameState.h"
#include "core/Rules.h"
#include "ui/BoardView.h"
#include "ui/Dialogs.h"
#include "ui/Display.h"
#include "ui/Panel.h"

using namespace std;

// 人机对战窗口。退出对局只返回主菜单，退出游戏统一走主菜单入口
LeaveMatch PVCWindow() {
    gShowExitButton = true; // 对局中显示退出按钮
    ui::EnsureDisplay();
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
        ui::Present();
        //让文字显示一段时间后在刷新棋盘
        Sleep(1000);
        DrawBoard();

        // --- 游戏主循环---
        while (!gameover) {
            for (int i = 0; i < playerSetTimes; i++) {
                settextstyle(20, 0, L"微软雅黑");
                settextcolor(RGB(255, 0, 0));
                ui::DrawPanelText(ui::kPanelX + ui::kPanelPadding, 10, 20, ui::kPanelAccent, L"你的回合，请落子");
                settextstyle(40, 0, L"微软雅黑");

                int col = -1, row = -1;

                //这是阻塞式等待循环，我们在这里插入按钮检测
                do {
                    m = ui::GetMouseMsgMapped(); // 阻塞，等待鼠标消息
                    // --- 检测是否点击了退出按钮 ---
                    if (m.uMsg == WM_LBUTTONDOWN && exitButton.IsMouseOnButton(m)) {
                        const ExitChoice choice = ShowExitConfirm();
                        if (choice == ExitChoice::BackToMenu) {
                            gShowExitButton = false;
                            return LeaveMatch::BackToMenu;
                        }
                        // 继续游戏：重绘棋盘和回合提示，继续等待落子
                        DrawBoard();
                        settextstyle(20, 0, L"微软雅黑");
                        settextcolor(RGB(255, 0, 0));
                        ui::DrawPanelText(ui::kPanelX + ui::kPanelPadding, 10, 20, ui::kPanelAccent, L"你的回合，请落子");
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
                            ui::DrawPanelText(ui::kPanelX + ui::kPanelPadding, 45, 17, ui::kPanelAccent, L"完成本回合后方可悔棋");
                        }

                    }

                    // 落在黑边/窗口外时坐标为 -1，不能当成 (0,0) 落子
                    if (m.uMsg == WM_LBUTTONDOWN && m.x >= 0 && m.y >= 0) {
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
                    DrawResultOverlay(L"黑棋胜利！");
                    gameover = true;
                    break; // 跳出 for 循环
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    DrawResultOverlay(L"和棋！");
                    gameover = true;
                    break; // 跳出 for 循环
                }
            }

            // 如果因为悔棋触发了 continue，那么下面的代码都不会执行
            if (gameover) break;

            settextstyle(20, 0, L"微软雅黑");
            settextcolor(RGB(255, 0, 0));
            ui::DrawPanelText(ui::kPanelX + ui::kPanelPadding, 10, 20, ui::kPanelAccent, L"AI思考中...");
            // 先把玩家刚落的子呈现出来，别让界面看起来“点了没反应”
            ui::Present();

            // AI 搜索比较慢，放到后台线程算：主线程继续泵消息/重绘，
            // 思考期间窗口和「退出」按钮依然可以响应，不会整页假死
            struct BoardSnapshot {
                int cells[BOARDSIZE][BOARDSIZE];
            };
            auto snapshot = std::make_shared<BoardSnapshot>();
            memcpy(snapshot->cells, board, sizeof(snapshot->cells));
            auto aiMovesPtr = std::make_shared<vector<pair<int, int>>>();
            auto aiDone = std::make_shared<atomic<bool>>(false);
            std::thread aiWorker([snapshot, aiMovesPtr, aiDone]() {
                *aiMovesPtr = aiput(snapshot->cells);
                aiDone->store(true);
            });

            bool leaveMatch = false;
            while (!aiDone->load()) {
                ui::Pump(); // 思考期间拖动窗口边框仍能实时缩放
                ExMessage message;
                while (ui::PeekMessageMapped(&message, EX_MOUSE)) {
                    if (message.message != WM_LBUTTONDOWN) {
                        continue;
                    }
                    MOUSEMSG probe;
                    memset(&probe, 0, sizeof(probe));
                    probe.uMsg = message.message;
                    probe.x = static_cast<short>(message.x);
                    probe.y = static_cast<short>(message.y);
                    if (!exitButton.IsMouseOnButton(probe)) {
                        continue;
                    }
                    const ExitChoice choice = ShowExitConfirm();
                    if (choice == ExitChoice::BackToMenu) {
                        leaveMatch = true;
                        break;
                    }
                    DrawBoard();
                    settextstyle(20, 0, L"微软雅黑");
                    settextcolor(RGB(255, 0, 0));
                    ui::DrawPanelText(ui::kPanelX + ui::kPanelPadding, 10, 20, ui::kPanelAccent, L"AI思考中...");
                    ui::Present();
                }
                if (leaveMatch) {
                    break;
                }
                Sleep(10);
            }
            if (leaveMatch) {
                aiWorker.detach(); // 提前离开：后台结果丢弃，写入的是 shared_ptr，不会悬空
                gShowExitButton = false;
                return LeaveMatch::BackToMenu;
            }
            aiWorker.join();
            vector<pair<int, int>> aiMoves = *aiMovesPtr;
            flushmessage(EX_MOUSE); // 思考期间的点击直接丢弃，避免思考完突然落子

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
                    DrawResultOverlay(L"白棋胜利！");
                    gameover = true;
                    break;
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    DrawResultOverlay(L"和棋！");
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
                    DrawResultOverlay(L"白棋胜利！");
                    gameover = true;
                    break;
                }
                else if (result == 3) {
                    settextstyle(40, 0, L"隶书");
                    settextcolor(RGB(255, 0, 0));
                    DrawResultOverlay(L"和棋！");
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
        // 选择重新开始时继续下一局
    }

    return LeaveMatch::BackToMenu; // 兜底：正常流程不会走到这里
}

