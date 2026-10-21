#include "modes/LanSession.h"

// 先引入 winsock2，再引入 graphics.h（其内部会包含 windows.h），避免 winsock1/2 冲突
#include "net/Net.h"
#include "net/OnlineGame.h"

#include "modes/LanUi.h"

#include "ui/Dialogs.h"
#include "ui/Display.h"
#include "ui/Panel.h"

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

    // 联机聊天：消息历史 + 输入状态
    std::vector<std::wstring> chatLog;
    bool chatting = false;
    std::wstring chatInput;

    // 聊天区布局与滚动状态（chatScroll = 从底部往上滚动的行数，0 表示停在最新消息）
    constexpr int kChatX = 618;
    constexpr int kChatW = 172;
    constexpr int kChatTop = 142;
    constexpr int kChatBottom = 424;
    constexpr int kChatBoxTop = 436;
    constexpr int kChatBoxBottom = 464;
    int chatScroll = 0;

    // 按聊天区宽度把一条消息拆成多行（字号 14）
    const auto wrapChatMessage = [](const std::wstring& item) {
        std::vector<std::wstring> lines;
        settextstyle(15, 0, kFaceYahei);
        std::wstring line;
        for (const wchar_t ch : item) {
            line.push_back(ch);
            if (textwidth(line.c_str()) > kChatW) {
                const wchar_t last = line.back();
                line.pop_back();
                if (!line.empty()) {
                    lines.push_back(line);
                }
                line.assign(1, last);
            }
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
        return lines;
    };

    // 右侧控制区（面板宽 200px）：按钮 160 宽，按面板宽度计算居中，左右各留 20px
    constexpr int kCtrlPanelX = kBoardPixels;
    constexpr int kCtrlPanelW = kWindowW - kBoardPixels;
    constexpr int kCtrlBtnW = 160;
    constexpr int kCtrlBtnH = 36;
    const int ctrlBtnX = kCtrlPanelX + (kCtrlPanelW - kCtrlBtnW) / 2;

    Button btnUndo = MakeButton(ctrlBtnX, 492, kCtrlBtnW, kCtrlBtnH, L"悔棋", 22);
    Button btnQuit = MakeButton(ctrlBtnX, 540, kCtrlBtnW, kCtrlBtnH, L"退出", 22);
    Button btnRestart = MakeButton(230, 340, 150, 40, L"再来一局", 26);
    Button btnBackMenu = MakeButton(428, 340, 150, 40, L"返回大厅", 26);
    Button btnAgree = MakeButton(230, 340, 150, 40, L"同意", 26);
    Button btnRefuse = MakeButton(428, 340, 150, 40, L"拒绝", 26);
    Button btnPeerLeftBack = MakeButton(329, 340, 150, 40, L"返回大厅", 26);

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

    const auto appendChat = [&](const std::wstring& line) {
        chatLog.push_back(line);
        if (chatLog.size() > 200) {
            chatLog.erase(chatLog.begin());
        }
        // 正在翻看旧消息时，保持当前阅读位置不被新消息顶走
        if (chatScroll > 0) {
            chatScroll += static_cast<int>(wrapChatMessage(line).size());
        }
    };
    const auto sendChat = [&]() {
        if (!chatInput.empty()) {
            const std::wstring text = chatInput;
            chatInput.clear();
            if (!channel.SendChat(Narrow(text))) {
                setToast(L"消息发送失败，连接可能已断开");
            }
            appendChat(L"我：" + text);
            chatScroll = 0; // 发完消息回到最新位置
        }
        chatting = false;
        dirty = true;
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
        case obnet::kMsgChat:
            appendChat(L"对方：" + Widen(message.text));
            dirty = true;
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

    // 右侧聊天区：输入框上方显示本局消息历史，可滚动查看更早的对话
    const auto drawChat = [&]() {
        const int chatX = kChatX;
        const int chatWidth = kChatW;
        const int boxTop = kChatBoxTop;
        const int boxBottom = kChatBoxBottom;
        constexpr int lineHeight = 19;

        std::vector<std::wstring> lines;
        for (const std::wstring& item : chatLog) {
            const std::vector<std::wstring> wrapped = wrapChatMessage(item);
            lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        }
        const int total = static_cast<int>(lines.size());
        const int maxLines = (kChatBottom - kChatTop) / lineHeight;
        const int maxScroll = total > maxLines ? total - maxLines : 0;
        if (chatScroll > maxScroll) {
            chatScroll = maxScroll;
        }
        if (chatScroll < 0) {
            chatScroll = 0;
        }
        const int startFromBottom = total - maxLines - chatScroll;
        const int start = startFromBottom > 0 ? startFromBottom : 0;
        // 只画可视区内的行，避免滚动后越界画到上方状态文字上
        const int visible = total - start > maxLines ? maxLines : total - start;
        int y = kChatBottom - visible * lineHeight;
        for (int i = start; i < start + visible; ++i) {
            DrawTextAt(chatX, y, 15, ui::kPanelText, lines[i].c_str());
            y += lineHeight;
        }

        // 内容超过一屏时在右缘画滚动条：滑块贴底表示停在最新消息
        if (maxScroll > 0) {
            const int trackH = kChatBottom - kChatTop;
            const int thumbH = trackH * maxLines / total;
            const int thumbY = kChatTop + (trackH - thumbH) * (maxScroll - chatScroll) / maxScroll;
            setfillcolor(ui::kPanelDivider);
            solidrectangle(chatX + kChatW - 3, kChatTop, chatX + kChatW - 1, kChatBottom);
            setfillcolor(ui::kPanelAccent);
            solidrectangle(chatX + kChatW - 3, thumbY, chatX + kChatW - 1, thumbY + thumbH);
        }

        setfillcolor(chatting ? RGB(34, 36, 42) : ui::kPanelBg);
        solidrectangle(chatX, boxTop, chatX + chatWidth, boxBottom);
        setlinecolor(chatting ? ui::kPanelAccent : ui::kPanelDivider);
        rectangle(chatX, boxTop, chatX + chatWidth, boxBottom);

        if (chatting) {
            DrawTextAt(chatX + 8, boxTop + 6, 15, ui::kPanelText, chatInput.c_str());
            settextstyle(15, 0, kFaceYahei);
            const int caretX = chatX + 8 + (chatInput.empty() ? 0 : textwidth(chatInput.c_str())) + 1;
            DrawTextAt(caretX, boxTop + 6, 15, ui::kPanelAccent, L"|");
        } else {
            DrawTextAt(chatX + 8, boxTop + 6, 15, ui::kPanelMuted, L"点击或回车输入");
        }
    };

    auto drawScene = [&]() {
        DrawBoardToBuffer(game);

        // 右侧状态区：深色面板上用浅色文字，回合/提示用 accent 红
        const int panelTextX = ui::kPanelX + ui::kPanelPadding;
        if (peerLeft) {
            DrawTextAt(panelTextX, 10, 20, ui::kPanelAccent, L"对方已断开");
        } else if (game.Over()) {
            DrawTextAt(panelTextX, 10, 20, ui::kPanelAccent, L"本局已结束");
        } else if (game.CurrentPlayer() == myColor) {
            DrawTextAt(panelTextX, 10, 20, ui::kPanelAccent, L"你的回合，请落子");
        } else {
            DrawTextAt(panelTextX, 10, 20, ui::kPanelAccent, L"等待对方落子…");
        }
        DrawTextAt(panelTextX, 42, 18, ui::kPanelText, myColor == 1 ? L"我方：黑棋" : L"我方：白棋");
        DrawTextAt(panelTextX, 68, 18, ui::kPanelText, peerColor == 1 ? L"对方：黑棋" : L"对方：白棋");
        if (!peerLeft && !game.Over() && game.CurrentPlayer() == myColor) {
            const std::wstring left = L"本回合剩余 " + std::to_wstring(game.StonesLeft()) + L" 子";
            DrawTextAt(panelTextX, 94, 18, ui::kPanelText, left.c_str());
        }
        if (!toast.empty()) {
            DrawTextAt(panelTextX, 120, 17, ui::kPanelAccent, toast.c_str());
        }
        drawChat();
        DrawTextAt(panelTextX, 588, 16, ui::kPanelMuted, L"Esc：退出对局");

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
        ui::Present();
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
        while (ui::PeekMessageMapped(&message, static_cast<BYTE>(EX_MOUSE | EX_KEY | EX_CHAR))) {
            if (message.message == WM_KEYDOWN) {
                if (chatting) {
                    if (message.vkcode == VK_ESCAPE) {
                        chatInput.clear();
                        chatting = false;
                        dirty = true;
                        continue;
                    }
                    if (message.vkcode == VK_RETURN) {
                        sendChat();
                        continue;
                    }
                    if (message.vkcode == VK_BACK) {
                        if (!chatInput.empty()) {
                            chatInput.pop_back();
                            dirty = true;
                        }
                        continue;
                    }
                    continue;
                }
                if (message.vkcode == VK_RETURN) {
                    chatting = true;
                    dirty = true;
                    continue;
                }
                if (message.vkcode == VK_PRIOR) { // PageUp：翻看更早的聊天记录
                    chatScroll += 8;
                    dirty = true;
                    continue;
                }
                if (message.vkcode == VK_NEXT) { // PageDown：回到更新的聊天记录
                    chatScroll -= 8;
                    if (chatScroll < 0) {
                        chatScroll = 0;
                    }
                    dirty = true;
                    continue;
                }
                if (message.vkcode == VK_ESCAPE) {
                    // 联机对局退出二次确认，避免误按 Esc 直接断开连接
                    if (ShowExitConfirm(L"返回大厅") == ExitChoice::BackToMenu) {
                        sendBye();
                        keepRunning = false;
                        break;
                    }
                    dirty = true;
                    continue;
                }
                continue;
            }
            if (message.message == WM_CHAR) {
                if (chatting) {
                    const wchar_t ch = message.ch;
                    if (ch >= 32 && chatInput.size() < 60) {
                        chatInput.push_back(ch);
                        dirty = true;
                    }
                }
                continue;
            }
            if (message.message == WM_MOUSEWHEEL) {
                // 滚轮在聊天区内生效：向上滚查看更早的消息
                POINT cursor;
                GetCursorPos(&cursor);
                ScreenToClient(GetHWnd(), &cursor);
                int wheelX = -1;
                int wheelY = -1;
                ui::MapWindowPoint(cursor.x, cursor.y, &wheelX, &wheelY);
                if (wheelX >= kChatX && wheelX <= kChatX + kChatW &&
                    wheelY >= kChatTop && wheelY <= kChatBoxBottom) {
                    chatScroll += message.wheel > 0 ? 4 : -4;
                    if (chatScroll < 0) {
                        chatScroll = 0;
                    }
                    dirty = true;
                }
                continue;
            }
            if (message.message != WM_LBUTTONDOWN) {
                continue;
            }
            const int mx = message.x;
            const int my = message.y;

            // 点击聊天区（消息区或输入框）直接进入输入状态，不必先按回车
            const bool inChatArea = mx >= kChatX && mx <= kChatX + kChatW &&
                                    my >= kChatTop && my <= kChatBoxBottom;
            if (inChatArea) {
                if (!chatting) {
                    chatting = true;
                    dirty = true;
                }
                continue;
            }
            if (chatting) {
                continue; // 输入状态下不响应棋盘/按钮点击，Esc 取消输入
            }
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
                    if (ShowExitConfirm(L"返回大厅") == ExitChoice::BackToMenu) {
                        sendBye();
                        keepRunning = false;
                        break;
                    }
                    dirty = true;
                }
                continue;
            }

            if (HitButton(btnQuit, mx, my)) {
                if (ShowExitConfirm(L"返回大厅") == ExitChoice::BackToMenu) {
                    sendBye();
                    keepRunning = false;
                    break;
                }
                dirty = true;
                continue;
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
        ui::Pump(); // 窗口大小变化时重绘，拖动边框缩放能实时跟随
        Sleep(15);
    }
}

