#include "OnlineGame.h"

#include <cstring>

namespace obgame {

Game::Game() : winner_(0) {
    Reset();
}

void Game::Reset() {
    std::memset(board_, 0, sizeof(board_));
    history_.clear();
    winner_ = 0;
}

int Game::StoneAt(int x, int y) const {
    if (x < 0 || x >= obnet::kBoardSize || y < 0 || y >= obnet::kBoardSize) {
        return -1;
    }
    return board_[y][x];
}

bool Game::CanPlace(int x, int y) const {
    if (winner_ != 0) {
        return false;
    }
    if (x < 0 || x >= obnet::kBoardSize || y < 0 || y >= obnet::kBoardSize) {
        return false;
    }
    return board_[y][x] == 0;
}

int Game::StartOfTurn(int turnIndex) {
    if (turnIndex <= 0) {
        return 0;
    }
    // 第 0 回合（黑第 1 手）1 子，之后每回合 2 子：
    // 回合起点序列 0, 1, 3, 5, 7, ...
    return 1 + 2 * (turnIndex - 1);
}

int Game::TurnIndexOfHistory(int historySize) {
    if (historySize <= 0) {
        return 0;
    }
    return (historySize + 1) / 2;
}

int Game::PlayerOfTurn(int turnIndex) {
    // 偶数回合黑棋，奇数回合白棋
    return (turnIndex % 2 == 0) ? 1 : 2;
}

int Game::StonesLeft() const {
    const int m = HistorySize();
    if (m <= 0) {
        return 1; // 黑棋第 1 回合只落 1 子（连六棋规则）
    }
    const int turn = TurnIndexOfHistory(m);
    return 2 - (m - StartOfTurn(turn));
}

int Game::UndoTarget() const {
    const int m = HistorySize();
    if (m <= 0) {
        return -1;
    }
    const int turn = TurnIndexOfHistory(m);
    if (turn < 2) {
        return -1; // 请求方还没有“上一回合”，不能悔棋
    }
    return StartOfTurn(turn - 2);
}

bool Game::IsTurnBoundary(int historySize) {
    if (historySize < 0) {
        return false;
    }
    if (historySize == 0) {
        return true;
    }
    return (historySize - 1) % 2 == 0;
}

bool Game::Place(int x, int y, int color) {
    if (winner_ != 0) {
        return false;
    }
    if (color != CurrentPlayer() || StonesLeft() <= 0 || !CanPlace(x, y)) {
        return false;
    }
    board_[y][x] = color;
    history_.push_back(Move{ x, y, color });

    if (CheckWin(x, y, color)) {
        winner_ = color;
    } else if (BoardFull()) {
        winner_ = 3;
    }
    return true;
}

bool Game::ApplyUndo(int target) {
    if (target < 0 || target >= HistorySize()) {
        return false;
    }
    if (!IsTurnBoundary(target)) {
        return false;
    }
    history_.resize(static_cast<size_t>(target));
    Rebuild();
    winner_ = 0;
    return true;
}

void Game::Rebuild() {
    std::memset(board_, 0, sizeof(board_));
    for (const Move& mv : history_) {
        if (mv.x >= 0 && mv.x < obnet::kBoardSize && mv.y >= 0 && mv.y < obnet::kBoardSize) {
            board_[mv.y][mv.x] = mv.color;
        }
    }
}

bool Game::CheckWin(int x, int y, int color) const {
    static const int dx[4] = { 1, 0, 1, 1 };
    static const int dy[4] = { 0, 1, 1, -1 };
    for (int d = 0; d < 4; ++d) {
        int count = 1;
        for (int step = 1; step <= 5; ++step) {
            const int nx = x + dx[d] * step;
            const int ny = y + dy[d] * step;
            if (nx < 0 || nx >= obnet::kBoardSize || ny < 0 || ny >= obnet::kBoardSize) {
                break;
            }
            if (board_[ny][nx] != color) {
                break;
            }
            ++count;
        }
        for (int step = 1; step <= 5; ++step) {
            const int nx = x - dx[d] * step;
            const int ny = y - dy[d] * step;
            if (nx < 0 || nx >= obnet::kBoardSize || ny < 0 || ny >= obnet::kBoardSize) {
                break;
            }
            if (board_[ny][nx] != color) {
                break;
            }
            ++count;
        }
        if (count >= 6) {
            return true;
        }
    }
    return false;
}

bool Game::BoardFull() const {
    for (int y = 0; y < obnet::kBoardSize; ++y) {
        for (int x = 0; x < obnet::kBoardSize; ++x) {
            if (board_[y][x] == 0) {
                return false;
            }
        }
    }
    return true;
}

} // namespace obgame
