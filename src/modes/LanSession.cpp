#include "modes/LanSession.h"

// 先引入 winsock2，再引入 graphics.h（其内部会包含 windows.h），避免 winsock1/2 冲突
#include "net/Net.h"
#include "net/OnlineGame.h"

#include "modes/LanUi.h"

#include <graphics.h>
#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

using obgame::Game;
using obnet::Msg;
using obnet::NetChannel;

void RunSession(NetChannel& channel, bool isHost) {
    const int myColor = isHost ? 1 : 2;
    const int peerColor = isHost ? 2 : 1;

    Game game;
    bool keepRunning = true;
    bool peerLeft = false;
    bool waitingUndoReply = false;
    bool peerUndoPending = false;
    int peerUndoTarget = -1;
    bool localRestartRequested = false;
    bool peerRestartRequested = false;
    std::wstring toast;
    DWORD toastUntil = 0;
    bool dirty = true;

    // 按钮位置与尺寸沿用原版右下角按钮习惯
    Button btnUndo = MakeButton(614, 496, 188, 34, L"悔棋", 24);
    Button btnQuit = MakeButton(614, 542, 188, 34, L"退出", 24);
    Button btnRestart = MakeButton(230, 340, 150, 40, L"再来一局", 26);
    Button btnBackMenu = MakeButton(428, 340, 150, 40, L"返回菜单", 26);
    Button btnAgree = MakeButton(230, 340, 150, 40, L"同意", 26);
    Button btnRefuse = MakeButton(428, 340, 150, 40, L"拒绝", 26);
    Button btnPeerLeftBack = MakeButton(329, 340, 150, 40, L"返回菜单", 26);

    auto setToast = [&](const wchar_t* text) {
        toast = text;
        toastUntil = GetTickCount() + 3000;
        dirty = true;
    };
    auto sendBye = [&]() {
        channel.Send(obnet::MakeMsg(obnet::kMsgBye));
    };
    auto doRestart = [&]() {
        game.Reset();
        waitingUndoReply = false;
        peerUndoPending = false;
        peerUndoTarget = -1;
        localRestartRequested = false;
        peerRestartRequested = false;
        setToast(L"新对局开始，主机制黑先手");
    };

    auto handleMessage = [&](const Msg& message) {
        switch (message.type) {
        case obnet::kMsgMove: {
            if (peerLeft || game.Over() || waitingUndoReply || peerUndoPending) {
                setToast(L"对局状态异常，连接结束");
                sendBye();
                peerLeft = true;
                break;
            }
            if (game.CurrentPlayer() == myColor) {
                setToast(L"对局状态异常，连接结束");
                sendBye();
                peerLeft = true;
                break;
            }
            if (!game.CanPlace(message.arg0, message.arg1)) {
                setToast(L"对方落子非法，连接结束");
                sendBye();
                peerLeft = true;
                break;
            }
            game.Place(message.arg0, message.arg1, peerColor);
            dirty = true;
            break;
        }
        case obnet::kMsgUndoRequest: {
            if (peerLeft || game.Over() || waitingUndoReply || peerUndoPending) {
                channel.Send(obnet::MakeMsg(obnet::kMsgUndoDeny));
                break;
            }
            const int target = game.UndoTarget();
            if (target < 0) {
                channel.Send(obnet::MakeMsg(obnet::kMsgUndoDeny));
                break;
            }
            peerUndoPending = true;
            peerUndoTarget = target;
            dirty = true;
            break;
        }
        case obnet::kMsgUndoAccept: {
            if (!waitingUndoReply) {
                break;
            }
            const int expected = game.UndoTarget();
            if (expected < 0 || message.arg0 != expected || !game.ApplyUndo(message.arg0)) {
                setToast(L"棋盘不同步，对局结束");
                sendBye();
                peerLeft = true;
                break;
            }
            waitingUndoReply = false;
            setToast(L"悔棋成功，请重新落子");
            break;
        }
        case obnet::kMsgUndoDeny:
            if (waitingUndoReply) {
                waitingUndoReply = false;
                setToast(L"对方拒绝了悔棋");
            }
            break;
        case obnet::kMsgRestartRequest:
            if (game.Over() && !peerLeft) {
                if (localRestartRequested) {
                    channel.Send(obnet::MakeMsg(obnet::kMsgRestartAccept));
                    doRestart();
                } else {
                    peerRestartRequested = true;
                    dirty = true;
                }
            }
            break;
        case obnet::kMsgRestartAccept:
            if (localRestartRequested) {
                doRestart();
            }
            break;
        case obnet::kMsgRestartDeny:
            if (localRestartRequested) {
                localRestartRequested = false;
                setToast(L"对方拒绝再来一局");
            }
            break;
        case obnet::kMsgBye:
            peerLeft = true;
            dirty = true;
            break;
        case obnet::kMsgPing:
            channel.Send(obnet::MakeMsg(obnet::kMsgPong));
            break;
        case obnet::kMsgHello:
        case obnet::kMsgPong:
        default:
            break;
        }
    };

    // 与原版一致的浅色弹框（白底黑框 + 隶书标题 + 微软雅黑正文）
    auto drawOverlayBox = [](const wchar_t* title, const wchar_t* subtitle) {
        setfillcolor(kWhite);
        solidrectangle(180, 210, 628, 410);
        setlinecolor(kTextBlack);
        rectangle(180, 210, 628, 410);
        rectangle(181, 211, 627, 409);
        DrawTextCentered(404, 232, 40, kTextBlack, title, kFaceLi);
        if (subtitle != nullptr && subtitle[0] != L'\0') {
            DrawTextCentered(404, 292, 20, kTextBlack, subtitle);
        }
    };

    auto drawScene = [&]() {
        DrawBoardToBuffer(game);

        // 右侧状态区：沿用原版红色提示 + 黑色说明文字
        if (peerLeft) {
            DrawTextAt(618, 10, 20, kTextRed, L"对方已断开");
        } else if (game.Over()) {
            DrawTextAt(618, 10, 20, kTextRed, L"本局已结束");
        } else if (game.CurrentPlayer() == myColor) {
            DrawTextAt(618, 10, 20, kTextRed, L"你的回合，请落子");
        } else {
            DrawTextAt(618, 10, 20, kTextRed, L"等待对方落子…");
        }
        DrawTextAt(618, 42, 18, kTextBlack, myColor == 1 ? L"我方：黑棋" : L"我方：白棋");
        DrawTextAt(618, 68, 18, kTextBlack, peerColor == 1 ? L"对方：黑棋" : L"对方：白棋");
        if (!peerLeft && !game.Over() && game.CurrentPlayer() == myColor) {
            const std::wstring left = L"本回合剩余 " + std::to_wstring(game.StonesLeft()) + L" 子";
            DrawTextAt(618, 94, 18, kTextBlack, left.c_str());
        }
        if (!toast.empty()) {
            DrawTextAt(618, 120, 17, kTextRed, toast.c_str());
        }
        DrawTextAt(618, 588, 16, kTextBlack, L"Esc：退出对局");

        if (peerLeft) {
            // 遮罩内提供按钮
        } else if (game.Over()) {
            // 结束遮罩内提供按钮
        } else if (peerUndoPending) {
            // 悔棋询问遮罩内提供按钮
        } else if (waitingUndoReply) {
            btnQuit.DrawButton();
        } else {
            btnUndo.DrawButton();
            btnQuit.DrawButton();
        }

        if (peerLeft) {
            drawOverlayBox(L"对方已离开对局", L"点击按钮返回大厅");
            btnPeerLeftBack.DrawButton();
        } else if (game.Over()) {
            const int winner = game.Winner();
            const wchar_t* title = (winner == 3) ? L"和棋" : (winner == myColor ? L"你赢了！" : L"你输了");
            std::wstring subtitle;
            if (localRestartRequested) {
                subtitle = L"已邀请对方再来一局…";
            } else if (peerRestartRequested) {
                subtitle = L"对方想再来一局";
            } else {
                subtitle = L"可以点击「再来一局」继续";
            }
            drawOverlayBox(title, subtitle.c_str());
            btnRestart.DrawButton();
            btnBackMenu.DrawButton();
        } else if (peerUndoPending) {
            drawOverlayBox(L"对方请求悔棋", L"是否同意回退到上一回合？");
            btnAgree.DrawButton();
            btnRefuse.DrawButton();
        } else if (waitingUndoReply) {
            drawOverlayBox(L"等待对方同意悔棋", L"按 Esc 可退出对局");
        }
    };

    drawScene();
    dirty = false;

    while (keepRunning) {
        std::vector<Msg> inbox;
        channel.PollAll(inbox);
        for (const Msg& message : inbox) {
            handleMessage(message);
        }
        if (!channel.Connected() && !peerLeft) {
            peerLeft = true;
            dirty = true;
        }
        if (!toast.empty() && GetTickCount() > toastUntil) {
            toast.clear();
            dirty = true;
        }

        ExMessage message;
        while (peekmessage(&message, static_cast<BYTE>(EX_MOUSE | EX_KEY))) {
            if (message.message == WM_KEYDOWN) {
                if (message.vkcode == VK_ESCAPE) {
                    sendBye();
                    keepRunning = false;
                    break;
                }
                continue;
            }
            if (message.message != WM_LBUTTONDOWN) {
                continue;
            }
            const int mx = message.x;
            const int my = message.y;

            if (peerLeft) {
                if (HitButton(btnPeerLeftBack, mx, my)) {
                    keepRunning = false;
                    break;
                }
                continue;
            }
            if (game.Over()) {
                if (HitButton(btnRestart, mx, my) && !localRestartRequested) {
                    localRestartRequested = true;
                    channel.Send(obnet::MakeMsg(obnet::kMsgRestartRequest));
                    if (peerRestartRequested) {
                        channel.Send(obnet::MakeMsg(obnet::kMsgRestartAccept));
                        doRestart();
                    } else {
                        setToast(L"已邀请对方再来一局");
                    }
                } else if (HitButton(btnBackMenu, mx, my)) {
                    sendBye();
                    keepRunning = false;
                    break;
                }
                continue;
            }
            if (peerUndoPending) {
                if (HitButton(btnAgree, mx, my)) {
                    channel.Send(obnet::MakeMsg(obnet::kMsgUndoAccept, peerUndoTarget));
                    if (!game.ApplyUndo(peerUndoTarget)) {
                        setToast(L"棋盘不同步，对局结束");
                        sendBye();
                        peerLeft = true;
                    } else {
                        setToast(L"已同意对方悔棋");
                    }
                    peerUndoPending = false;
                    peerUndoTarget = -1;
                    dirty = true;
                } else if (HitButton(btnRefuse, mx, my)) {
                    channel.Send(obnet::MakeMsg(obnet::kMsgUndoDeny));
                    peerUndoPending = false;
                    peerUndoTarget = -1;
                    setToast(L"已拒绝对方悔棋");
                }
                continue;
            }
            if (waitingUndoReply) {
                if (HitButton(btnQuit, mx, my)) {
                    sendBye();
                    keepRunning = false;
                    break;
                }
                continue;
            }

            if (HitButton(btnQuit, mx, my)) {
                sendBye();
                keepRunning = false;
                break;
            }
            if (game.CurrentPlayer() != myColor) {
                // 对方回合：点击棋盘或悔棋时给出提示，避免“点了没反应”
                const bool onBoard = mx >= 0 && mx < kBoardPixels && my >= 0 && my < kBoardPixels;
                if (onBoard || HitButton(btnUndo, mx, my)) {
                    setToast(L"对方回合，请稍候");
                }
                continue;
            }
            if (HitButton(btnUndo, mx, my)) {
                const int target = game.UndoTarget();
                if (target < 0) {
                    setToast(L"当前回合无法悔棋");
                } else {
                    channel.Send(obnet::MakeMsg(obnet::kMsgUndoRequest));
                    waitingUndoReply = true;
                    setToast(L"已请求悔棋，等待同意");
                }
                continue;
            }
            if (mx >= 0 && mx < kBoardPixels && my >= 0 && my < kBoardPixels) {
                const int bx = mx / kCellSize;
                const int by = my / kCellSize;
                if (game.CanPlace(bx, by)) {
                    game.Place(bx, by, myColor);
                    channel.Send(obnet::MakeMsg(obnet::kMsgMove, bx, by));
                    dirty = true;
                } else {
                    setToast(L"该位置已有棋子");
                }
            }
        }
        if (!keepRunning) {
            break;
        }

        if (dirty) {
            drawScene();
            dirty = false;
        }
        Sleep(15);
    }
}

