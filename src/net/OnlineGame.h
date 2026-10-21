// 局域网对战 —— 对局核心（棋盘 / 回合 / 胜负 / 悔棋回滚）
// 不依赖 EasyX，可单独做单元测试；两端用同一套逻辑，保证棋盘状态一致。
#pragma once

#include <vector>

#include "Protocol.h"

namespace obgame {

struct Move {
    int x;
    int y;
    int color; // 1 黑 / 2 白
};

class Game {
public:
    Game();

    void Reset();

    int StoneAt(int x, int y) const;
    bool CanPlace(int x, int y) const;
    // 仅当轮到 color 且本回合还有剩余子时落子；成功返回 true
    bool Place(int x, int y, int color);

    int HistorySize() const { return static_cast<int>(history_.size()); }
    const std::vector<Move>& History() const { return history_; }

    int CurrentPlayer() const { return PlayerOfTurn(TurnIndexOfHistory(HistorySize())); }
    int StonesLeft() const;
    int Winner() const { return winner_; } // 0 未结束 / 1 黑胜 / 2 白胜 / 3 和棋
    bool Over() const { return winner_ != 0; }

    // 当前该落子一方“悔棋”时，应当回滚到的历史长度；-1 表示不可悔棋
    int UndoTarget() const;
    static bool IsTurnBoundary(int historySize);
    bool ApplyUndo(int target);

    // 回合换算（公开以便测试）
    static int StartOfTurn(int turnIndex);
    static int TurnIndexOfHistory(int historySize);
    static int PlayerOfTurn(int turnIndex);

private:
    void Rebuild();
    bool CheckWin(int x, int y, int color) const;
    bool BoardFull() const;

    int board_[obnet::kBoardSize][obnet::kBoardSize];
    std::vector<Move> history_;
    int winner_;
};

} // namespace obgame
