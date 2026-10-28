// 连六棋 AI 底层引擎：棋盘、6 格窗口线表、增量威胁统计。
//
// 设计要点（与旧版“逐格棋形分”完全不同）：
//   * 连六棋的战术本质由长度为 6 的窗口决定：
//       - 6 窗内 5 颗己方 + 1 空  -> 成六点（下一子直接赢）
//       - 6 窗内 4 颗己方 + 2 空  -> 四窗（这一回合两子可以赢）
//       - 6 窗内 3 颗己方 + 3 空  -> 三窗（发展潜力）
//   * 所有窗口的计数在 make/unmake 时增量维护，因此评估和威胁判定都是 O(1)
//     到 O(小常数)，不需要每个节点重扫全盘。
//   * 8 条方向线在 19x19 上等价于 4 个轴线方向的全部最大线段，共 924 个 6 窗。
#pragma once

#include <cstdint>
#include <cstring>

#include "core/GameTypes.h"

namespace c6 {

constexpr int kBoard = BOARDSIZE;              // 19
constexpr int kCells = kBoard * kBoard;        // 361
constexpr int kWindows = 924;                  // 19*14*2 + (2*(1+..+13)+14)*2
constexpr int kMaxCellWindows = 24;            // 每个格子最多落在 4*6 个窗口里
constexpr int kWinValue = 1000000000;          // 必胜分（带 ply 修正）
constexpr int kInfValue = 1100000000;

// 四个轴线方向
inline constexpr int kDirX[4] = { 1, 0, 1, 1 };
inline constexpr int kDirY[4] = { 0, 1, 1, -1 };

inline int CellX(int idx) { return idx % kBoard; }
inline int CellY(int idx) { return idx / kBoard; }
inline int CellIndex(int x, int y) { return y * kBoard + x; }
inline bool InBoard(int x, int y) { return x >= 0 && x < kBoard && y >= 0 && y < kBoard; }

struct Tables {
    int16_t winCells[kWindows][6];       // 窗口内的 6 个格子
    int16_t cellWinIds[kCells][kMaxCellWindows];
    uint8_t cellWinPos[kCells][kMaxCellWindows];
    uint8_t cellWinCount[kCells];
    uint64_t zobrist[3][kCells];
    uint64_t sideKey[3];
    uint64_t leftKey[3];
};

inline uint64_t SplitMix64(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline bool BuildTables(Tables& t) {
    std::memset(&t, 0, sizeof(t));

    // --- 生成所有最大线段上的 6 窗 ---
    int windowCount = 0;
    for (int y = 0; y < kBoard; ++y) {
        for (int x = 0; x < kBoard; ++x) {
            for (int d = 0; d < 4; ++d) {
                const int px = x - kDirX[d];
                const int py = y - kDirY[d];
                if (InBoard(px, py)) {
                    continue; // 不是线段起点
                }
                int line[kBoard];
                int len = 0;
                int cx = x;
                int cy = y;
                while (InBoard(cx, cy)) {
                    line[len++] = CellIndex(cx, cy);
                    cx += kDirX[d];
                    cy += kDirY[d];
                }
                for (int s = 0; s + 6 <= len; ++s) {
                    if (windowCount >= kWindows) {
                        return false;
                    }
                    for (int i = 0; i < 6; ++i) {
                        t.winCells[windowCount][i] = static_cast<int16_t>(line[s + i]);
                    }
                    ++windowCount;
                }
            }
        }
    }
    if (windowCount != kWindows) {
        return false;
    }

    // --- 反向索引：每个格子落在哪些窗口的哪个位置 ---
    for (int w = 0; w < kWindows; ++w) {
        for (int i = 0; i < 6; ++i) {
            const int cell = t.winCells[w][i];
            const int slot = t.cellWinCount[cell];
            if (slot >= kMaxCellWindows) {
                return false;
            }
            t.cellWinIds[cell][slot] = static_cast<int16_t>(w);
            t.cellWinPos[cell][slot] = static_cast<uint8_t>(i);
            ++t.cellWinCount[cell];
        }
    }

    // --- Zobrist ---
    uint64_t seed = 0x123456789ABCDEF0ULL;
    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < kCells; ++i) {
            t.zobrist[c][i] = SplitMix64(seed);
        }
    }
    for (int c = 0; c < 3; ++c) {
        t.sideKey[c] = SplitMix64(seed);
        t.leftKey[c] = SplitMix64(seed);
    }
    return true;
}

inline const Tables& GetTables() {
    static Tables t;
    static const bool ok = BuildTables(t);
    (void)ok;
    return t;
}

struct WindowState {
    int16_t cnt[3]; // 1/2 各自棋子数
    uint8_t occ;    // 6 bit，1 表示该位置有子
};

// 一个窗口对某一方的分类
struct WindowClass {
    int own;
    int opp;
    int winPos;  // 成六点在该窗口中的位置；-1 表示没有
    bool four;   // 是否为四窗（4 己 + 2 空）
};

inline WindowClass ClassifyWindow(const WindowState& ws, int color) {
    WindowClass c;
    c.own = ws.cnt[color];
    c.opp = ws.cnt[3 - color];
    c.winPos = -1;
    c.four = false;
    if (c.opp == 0) {
        if (c.own == 5) {
            const uint8_t emptyBits = static_cast<uint8_t>(~ws.occ & 0x3F);
            for (int i = 0; i < 6; ++i) {
                if (emptyBits & (1u << i)) {
                    c.winPos = i;
                    break;
                }
            }
        }
        else if (c.own == 4) {
            c.four = true;
        }
    }
    return c;
}

// 棋盘状态（含增量威胁统计）。只有一份，通过 Make/Unmake 修改。
struct Position {
    uint8_t cell[kCells];      // 0 空，1 黑，2 白
    uint8_t nearCnt[kCells];   // 切比雪夫距离 2 内的棋子数（候选点用）
    int side;                  // 当前行棋方
    int left;                  // 本回合还剩几子（1 或 2）
    int stones;
    uint64_t hash;             // 只含棋子的 Zobrist

    WindowState win[kWindows];

    // 成六点（按格去重）
    uint16_t cellWinCnt[3][kCells];
    int16_t winList[3][kCells];
    int16_t winListLen[3];
    int16_t winSlot[3][kCells];

    // 四窗（按窗口去重）
    int16_t fourList[3][kWindows];
    int16_t fourListLen[3];
    int16_t fourSlot[3][kWindows];

    // “对手没有子”的窗口按己方子数计数：catCnt[c][k]，k=0..5
    int16_t catCnt[3][6];
};

inline void ClearThreatState(Position& p) {
    std::memset(p.win, 0, sizeof(p.win));
    std::memset(p.cellWinCnt, 0, sizeof(p.cellWinCnt));
    std::memset(p.winSlot, 0xFF, sizeof(p.winSlot));
    std::memset(p.fourSlot, 0xFF, sizeof(p.fourSlot));
    std::memset(p.catCnt, 0, sizeof(p.catCnt));
    std::memset(p.winList, 0, sizeof(p.winList));
    std::memset(p.fourList, 0, sizeof(p.fourList));
    for (int c = 0; c < 3; ++c) {
        p.winListLen[c] = 0;
        p.fourListLen[c] = 0;
    }
}

inline void AddWinCell(Position& p, int color, int cell) {
    const int cnt = ++p.cellWinCnt[color][cell];
    if (cnt == 1) {
        p.winSlot[color][cell] = p.winListLen[color];
        p.winList[color][p.winListLen[color]++] = static_cast<int16_t>(cell);
    }
}

inline void RemoveWinCell(Position& p, int color, int cell) {
    const int cnt = --p.cellWinCnt[color][cell];
    if (cnt == 0) {
        const int slot = p.winSlot[color][cell];
        const int last = --p.winListLen[color];
        const int moved = p.winList[color][last];
        p.winList[color][slot] = static_cast<int16_t>(moved);
        p.winSlot[color][moved] = static_cast<int16_t>(slot);
        p.winSlot[color][cell] = -1;
    }
}

inline void AddFourWindow(Position& p, int color, int w) {
    p.fourSlot[color][w] = p.fourListLen[color];
    p.fourList[color][p.fourListLen[color]++] = static_cast<int16_t>(w);
}

inline void RemoveFourWindow(Position& p, int color, int w) {
    const int slot = p.fourSlot[color][w];
    const int last = --p.fourListLen[color];
    const int moved = p.fourList[color][last];
    p.fourList[color][slot] = static_cast<int16_t>(moved);
    p.fourSlot[color][moved] = static_cast<int16_t>(slot);
    p.fourSlot[color][w] = -1;
}

inline void ApplyWindowDelta(Position& p, int w, int color, const WindowClass& before, const WindowClass& after) {
    const Tables& t = GetTables();
    const int oldOwn = (before.opp == 0 && before.own >= 0 && before.own < 6) ? before.own : -1;
    const int newOwn = (after.opp == 0 && after.own >= 0 && after.own < 6) ? after.own : -1;
    if (oldOwn >= 0) {
        --p.catCnt[color][oldOwn];
    }
    if (newOwn >= 0) {
        ++p.catCnt[color][newOwn];
    }

    if (before.winPos < 0 && after.winPos >= 0) {
        AddWinCell(p, color, t.winCells[w][after.winPos]);
    }
    else if (before.winPos >= 0 && after.winPos < 0) {
        RemoveWinCell(p, color, t.winCells[w][before.winPos]);
    }

    if (!before.four && after.four) {
        AddFourWindow(p, color, w);
    }
    else if (before.four && !after.four) {
        RemoveFourWindow(p, color, w);
    }
}

inline void UpdateWindow(Position& p, int w, int color, int delta, int bitPos) {
    WindowState& ws = p.win[w];
    WindowClass before[3];
    for (int c = 1; c <= 2; ++c) {
        before[c] = ClassifyWindow(ws, c);
    }
    ws.cnt[color] = static_cast<int16_t>(ws.cnt[color] + delta);
    if (delta > 0) {
        ws.occ = static_cast<uint8_t>(ws.occ | (1u << bitPos));
    }
    else {
        ws.occ = static_cast<uint8_t>(ws.occ & ~(1u << bitPos));
    }
    for (int c = 1; c <= 2; ++c) {
        const WindowClass after = ClassifyWindow(ws, c);
        ApplyWindowDelta(p, w, c, before[c], after);
    }
}

inline void UpdateNear(Position& p, int x, int y, int delta) {
    for (int dy = -2; dy <= 2; ++dy) {
        const int ny = y + dy;
        if (ny < 0 || ny >= kBoard) {
            continue;
        }
        for (int dx = -2; dx <= 2; ++dx) {
            const int nx = x + dx;
            if (nx < 0 || nx >= kBoard) {
                continue;
            }
            p.nearCnt[CellIndex(nx, ny)] = static_cast<uint8_t>(p.nearCnt[CellIndex(nx, ny)] + delta);
        }
    }
}

// 落子（颜色取当前行棋方），并维护回合状态。返回落子方颜色。
inline int Make(Position& p, int cellIdx) {
    const Tables& t = GetTables();
    const int color = p.side;
    p.cell[cellIdx] = static_cast<uint8_t>(color);
    ++p.stones;
    p.hash ^= t.zobrist[color][cellIdx];
    UpdateNear(p, CellX(cellIdx), CellY(cellIdx), +1);
    const int n = t.cellWinCount[cellIdx];
    for (int i = 0; i < n; ++i) {
        UpdateWindow(p, t.cellWinIds[cellIdx][i], color, +1, t.cellWinPos[cellIdx][i]);
    }
    --p.left;
    if (p.left == 0) {
        p.side = 3 - p.side;
        p.left = 2;
    }
    return color;
}

// 撤销落子。cellIdx 必须是最后一步。
inline void Unmake(Position& p, int cellIdx) {
    const Tables& t = GetTables();
    const int color = p.cell[cellIdx];
    // 还原回合状态
    if (color == p.side) {
        ++p.left;
    }
    else {
        p.side = color;
        p.left = 1;
    }
    const int n = t.cellWinCount[cellIdx];
    for (int i = 0; i < n; ++i) {
        UpdateWindow(p, t.cellWinIds[cellIdx][i], color, -1, t.cellWinPos[cellIdx][i]);
    }
    UpdateNear(p, CellX(cellIdx), CellY(cellIdx), -1);
    p.hash ^= t.zobrist[color][cellIdx];
    --p.stones;
    p.cell[cellIdx] = 0;
}

// 由外部棋盘初始化
inline void InitPosition(Position& p, const int board[kBoard][kBoard], int side) {
    const Tables& t = GetTables();
    std::memset(&p, 0, sizeof(p));
    p.side = (side == 1) ? 1 : 2;
    p.left = 2;
    std::memset(p.winSlot, 0xFF, sizeof(p.winSlot));
    std::memset(p.fourSlot, 0xFF, sizeof(p.fourSlot));

    for (int y = 0; y < kBoard; ++y) {
        for (int x = 0; x < kBoard; ++x) {
            const int v = board[y][x];
            if (v != 1 && v != 2) {
                continue;
            }
            const int idx = CellIndex(x, y);
            p.cell[idx] = static_cast<uint8_t>(v);
            ++p.stones;
            p.hash ^= t.zobrist[v][idx];
        }
    }
    // 黑棋第一步只落一子：棋盘空且轮到黑棋时，本回合只剩 1 子
    if (p.stones == 0 && p.side == 1) {
        p.left = 1;
    }

    for (int y = 0; y < kBoard; ++y) {
        for (int x = 0; x < kBoard; ++x) {
            const int idx = CellIndex(x, y);
            if (p.cell[idx] == 0) {
                continue;
            }
            UpdateNear(p, x, y, +1);
        }
    }
    for (int w = 0; w < kWindows; ++w) {
        for (int i = 0; i < 6; ++i) {
            const int cell = t.winCells[w][i];
            const int color = p.cell[cell];
            if (color == 0) {
                continue;
            }
            p.win[w].cnt[color] = static_cast<int16_t>(p.win[w].cnt[color] + 1);
            p.win[w].occ = static_cast<uint8_t>(p.win[w].occ | (1u << i));
        }
        for (int c = 1; c <= 2; ++c) {
            const WindowClass cls = ClassifyWindow(p.win[w], c);
            if (cls.opp == 0 && cls.own < 6) {
                ++p.catCnt[c][cls.own];
            }
            if (cls.winPos >= 0) {
                AddWinCell(p, c, t.winCells[w][cls.winPos]);
            }
            if (cls.four) {
                AddFourWindow(p, c, w);
            }
        }
    }
}

inline uint64_t ZobristKey(const Position& p) {
    const Tables& t = GetTables();
    return p.hash ^ t.sideKey[p.side] ^ t.leftKey[p.left];
}

// 取某个四窗的两个空位；返回空位个数
inline int FourWindowEmpties(const Position& p, int w, int out[2]) {
    const Tables& t = GetTables();
    const uint8_t occ = p.win[w].occ;
    int n = 0;
    for (int i = 0; i < 6; ++i) {
        if ((occ & (1u << i)) == 0) {
            out[n++] = t.winCells[w][i];
            if (n == 2) {
                break;
            }
        }
    }
    return n;
}

// 局面评估：返回“当前行棋方”视角的分数
int Evaluate(const Position& p, int ply);

// 为了挡住对方的立即威胁（成六点 + 四窗），至少需要占几个格子；
// 返回所需格子数（可能 > k 表示挡不住），cover 可为空。
int DefenseNeed(const Position& p, int color, int k, int* cover);

// 收集对方所有立即威胁相关的格子（成六点 + 四窗空位）的并集
int CollectThreatCells(const Position& p, int color, int* out, int maxOut);

// 单个格子作为当前行棋方落子的启发分（走法排序用）
int CellOrderScore(const Position& p, int cell, int color);

} // namespace c6
