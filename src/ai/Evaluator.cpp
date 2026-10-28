#include "ai/Evaluator.h"

#include <cstdlib>

#include "ai/Engine.h"

using namespace c6;

namespace {

inline bool ContainsCell(const int* list, int n, int cell) {
    for (int i = 0; i < n; ++i) {
        if (list[i] == cell) {
            return true;
        }
    }
    return false;
}

// 单个空位对某条线形成的棋形价值（走法排序用的启发值，不是局面评估）
inline int PatternValue(int run, bool openLeft, bool openRight) {
    const int open = (openLeft ? 1 : 0) + (openRight ? 1 : 0);
    if (run >= 6) {
        return 300000;
    }
    if (run == 5) {
        return 40000;
    }
    if (run == 4) {
        return open * 5000;
    }
    if (run == 3) {
        return open * 600;
    }
    if (run == 2) {
        return open * 40;
    }
    return open * 4;
}

// 假设 color 在 cell 落子，沿某个方向的棋形价值
inline int DirectionValue(const Position& p, int x, int y, int d, int color) {
    const int dx = kDirX[d];
    const int dy = kDirY[d];
    int a = 0;
    int b = 0;
    int cx = x + dx;
    int cy = y + dy;
    while (InBoard(cx, cy) && p.cell[CellIndex(cx, cy)] == color) {
        ++a;
        cx += dx;
        cy += dy;
    }
    const bool openRight = InBoard(cx, cy) && p.cell[CellIndex(cx, cy)] == 0;

    cx = x - dx;
    cy = y - dy;
    while (InBoard(cx, cy) && p.cell[CellIndex(cx, cy)] == color) {
        ++b;
        cx -= dx;
        cy -= dy;
    }
    const bool openLeft = InBoard(cx, cy) && p.cell[CellIndex(cx, cy)] == 0;

    // 注意：只在候选点为空时调用，中心格算 1
    return PatternValue(a + b + 1, openLeft, openRight);
}

} // namespace

namespace c6 {

int CellOrderScore(const Position& p, int cell, int color) {
    const int x = CellX(cell);
    const int y = CellY(cell);
    int score = 0;
    for (int d = 0; d < 4; ++d) {
        score += DirectionValue(p, x, y, d, color) * 2;
        score += DirectionValue(p, x, y, d, 3 - color);
    }
    // 略微偏向中心
    score += 18 - (std::abs(x - kBoard / 2) + std::abs(y - kBoard / 2));
    return score;
}

int CollectThreatCells(const Position& p, int color, int* out, int maxOut) {
    int n = 0;
    for (int i = 0; i < p.winListLen[color] && n < maxOut; ++i) {
        const int cell = p.winList[color][i];
        if (!ContainsCell(out, n, cell)) {
            out[n++] = cell;
        }
    }
    const int nFour = p.fourListLen[color];
    for (int i = 0; i < nFour && n < maxOut; ++i) {
        int empties[2];
        if (FourWindowEmpties(p, p.fourList[color][i], empties) != 2) {
            continue;
        }
        for (int e = 0; e < 2; ++e) {
            if (!ContainsCell(out, n, empties[e]) && n < maxOut) {
                out[n++] = empties[e];
            }
        }
    }
    return n;
}

int DefenseNeed(const Position& p, int color, int k, int* cover) {
    const int nWin = p.winListLen[color];
    if (nWin > k) {
        return nWin;
    }
    int sel[3];
    int nSel = 0;
    for (int i = 0; i < nWin && nSel < 3; ++i) {
        sel[nSel++] = p.winList[color][i];
    }
    if (cover != nullptr) {
        for (int i = 0; i < nSel; ++i) {
            cover[i] = sel[i];
        }
    }

    const int nFour = p.fourListLen[color];
    if (nFour == 0) {
        return nWin;
    }

    // 收集没有被成六点封住的四窗
    constexpr int kMaxUncovered = 96;
    int emptyA[kMaxUncovered];
    int emptyB[kMaxUncovered];
    int uncovered = 0;
    for (int i = 0; i < nFour; ++i) {
        int empties[2];
        if (FourWindowEmpties(p, p.fourList[color][i], empties) != 2) {
            continue;
        }
        if (ContainsCell(sel, nSel, empties[0]) || ContainsCell(sel, nSel, empties[1])) {
            continue;
        }
        if (uncovered >= kMaxUncovered) {
            return k + 1; // 威胁太多，保守认为挡不住
        }
        emptyA[uncovered] = empties[0];
        emptyB[uncovered] = empties[1];
        ++uncovered;
    }
    if (uncovered == 0) {
        return nWin;
    }

    const int extraSlots = k - nSel;
    if (extraSlots <= 0) {
        return k + 1;
    }

    // 候选补位格子
    int cand[64];
    int nCand = 0;
    for (int i = 0; i < uncovered && nCand < 64; ++i) {
        const int cells[2] = { emptyA[i], emptyB[i] };
        for (int e = 0; e < 2; ++e) {
            if (ContainsCell(sel, nSel, cells[e]) || ContainsCell(cand, nCand, cells[e])) {
                continue;
            }
            if (nCand < 64) {
                cand[nCand++] = cells[e];
            }
        }
    }

    // 1 个补位
    for (int i = 0; i < nCand; ++i) {
        bool all = true;
        for (int j = 0; j < uncovered; ++j) {
            if (cand[i] != emptyA[j] && cand[i] != emptyB[j]) {
                all = false;
                break;
            }
        }
        if (all) {
            if (cover != nullptr) {
                cover[nSel] = cand[i];
            }
            return nSel + 1;
        }
    }

    // 2 个补位
    if (extraSlots >= 2) {
        for (int i = 0; i < nCand; ++i) {
            for (int j = i + 1; j < nCand; ++j) {
                bool all = true;
                for (int u = 0; u < uncovered; ++u) {
                    const int a = emptyA[u];
                    const int b = emptyB[u];
                    if (cand[i] != a && cand[i] != b && cand[j] != a && cand[j] != b) {
                        all = false;
                        break;
                    }
                }
                if (all) {
                    if (cover != nullptr) {
                        cover[nSel] = cand[i];
                        cover[nSel + 1] = cand[j];
                    }
                    return nSel + 2;
                }
            }
        }
    }
    return k + 1;
}

int Evaluate(const Position& p, int ply) {
    const int me = p.side;
    const int opp = 3 - me;

    // 我这一回合能直接赢？
    if (p.winListLen[me] > 0) {
        return kWinValue - ply;
    }
    if (p.left >= 2 && p.fourListLen[me] > 0) {
        return kWinValue - ply;
    }

    // 对方下一回合能否必杀？我这一回合挡得住吗？
    const int need = DefenseNeed(p, opp, p.left, nullptr);
    if (need > p.left) {
        return -kWinValue + ply;
    }

    static const int kWeight[6] = { 0, 1, 4, 18, 130, 900 };
    int score = 0;
    for (int c = 1; c <= 2; ++c) {
        const int sign = (c == me) ? 1 : -1;
        for (int j = 1; j <= 5; ++j) {
            score += sign * static_cast<int>(p.catCnt[c][j]) * kWeight[j];
        }
    }
    if (need > 0) {
        score -= 400 * need;
    }

    int center = 0;
    for (int i = 0; i < kCells; ++i) {
        const int v = p.cell[i];
        if (v == 0) {
            continue;
        }
        const int d = std::abs(CellX(i) - kBoard / 2) + std::abs(CellY(i) - kBoard / 2);
        center += (v == me) ? (kBoard - d) : -(kBoard - d);
    }
    score += center * 2;

    if (score > 900000) {
        score = 900000;
    }
    else if (score < -900000) {
        score = -900000;
    }
    return score;
}

} // namespace c6
