#include "ai/Search.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "ai/Engine.h"

using namespace c6;

namespace {

struct TTEntry {
    uint64_t key = 0;
    int32_t value = 0;
    int16_t depth = 0;
    uint8_t flag = 0; // 0 exact, 1 lower, 2 upper
    int16_t move = -1;
    uint32_t gen = 0;
};

constexpr int kTTSize = 1 << 19;
constexpr int kTTMask = kTTSize - 1;
TTEntry gTT[kTTSize];
uint32_t gGen = 1;

int gTimeLimitMs = 500;
int gMaxDepth = 20;
ai::SearchStats gLastStats;
std::mutex gAiputMutex; // PveMode 可能遗留一个 detach 的思考线程，串行化避免 TT 数据竞争

struct Ctx {
    std::chrono::steady_clock::time_point start;
    long long nodes = 0;
    int timeLimitMs = 500;
    int maxDepth = 20;
    bool stopped = false;
    bool timedOut = false;
};

inline long long ElapsedMs(const Ctx& c) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - c.start)
        .count();
}

inline bool CheckTime(Ctx& c) {
    if (c.stopped) {
        return true;
    }
    if (ElapsedMs(c) >= c.timeLimitMs) {
        c.stopped = true;
        c.timedOut = true;
        return true;
    }
    return false;
}

constexpr int kMaxMoves = 64;

struct Candidate {
    int cell;
    int score;
};

int GenerateMoves(const Position& p, int* out, int maxMoves, int ttMove) {
    const int me = p.side;
    const int opp = 3 - me;
    int n = 0;
    const auto push = [&](int cell) {
        if (n >= maxMoves) {
            return;
        }
        for (int i = 0; i < n; ++i) {
            if (out[i] == cell) {
                return;
            }
        }
        out[n++] = cell;
    };

    // 直接赢：只考虑成六点
    if (p.winListLen[me] > 0) {
        for (int i = 0; i < p.winListLen[me]; ++i) {
            push(p.winList[me][i]);
        }
        return n;
    }
    // 一回合两子能赢：四窗的两个空位就是答案
    if (p.left >= 2 && p.fourListLen[me] > 0) {
        for (int i = 0; i < p.fourListLen[me] && n < maxMoves; ++i) {
            int empties[2];
            if (FourWindowEmpties(p, p.fourList[me][i], empties) == 2) {
                push(empties[0]);
                push(empties[1]);
            }
        }
        return n;
    }

    const int need = DefenseNeed(p, opp, p.left, nullptr);
    int threatCells[64];
    const int nThreat = CollectThreatCells(p, opp, threatCells, 64);

    // 本回合每个子都必须用来挡：只生成威胁格
    if (need > 0 && need >= p.left) {
        for (int i = 0; i < nThreat; ++i) {
            push(threatCells[i]);
        }
        return n;
    }

    if (ttMove >= 0 && p.cell[ttMove] == 0) {
        push(ttMove);
    }
    for (int i = 0; i < nThreat; ++i) {
        push(threatCells[i]);
    }

    // 候选点：棋子周围两格内的空位
    constexpr int kPrefilter = 40;
    Candidate cheap[kCells];
    int nCheap = 0;
    for (int i = 0; i < kCells; ++i) {
        if (p.cell[i] != 0 || p.nearCnt[i] == 0) {
            continue;
        }
        const int x = CellX(i);
        const int y = CellY(i);
        int s = 0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const int nx = x + dx;
                const int ny = y + dy;
                if (!InBoard(nx, ny)) {
                    continue;
                }
                const int v = p.cell[CellIndex(nx, ny)];
                if (v == me) {
                    s += 4;
                }
                else if (v != 0) {
                    s += 3;
                }
            }
        }
        s += 18 - (std::abs(x - kBoard / 2) + std::abs(y - kBoard / 2));
        cheap[nCheap].cell = i;
        cheap[nCheap].score = s;
        ++nCheap;
    }
    if (nCheap > kPrefilter) {
        std::partial_sort(cheap, cheap + kPrefilter, cheap + nCheap,
                          [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
        nCheap = kPrefilter;
    }
    Candidate detail[kCells];
    int nDetail = 0;
    for (int i = 0; i < nCheap; ++i) {
        detail[nDetail].cell = cheap[i].cell;
        detail[nDetail].score = CellOrderScore(p, cheap[i].cell, me);
        ++nDetail;
    }
    std::sort(detail, detail + nDetail,
              [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    for (int i = 0; i < nDetail; ++i) {
        push(detail[i].cell);
    }

    // 空盘兜底：只允许把真正为空的格子放进走法表；棋盘下满时返回 0（和棋）
    if (n == 0) {
        const int center = CellIndex(kBoard / 2, kBoard / 2);
        if (p.cell[center] == 0) {
            push(center);
        }
        for (int i = 0; i < kCells && n < maxMoves; ++i) {
            if (p.cell[i] == 0) {
                push(i);
            }
        }
    }
    return n;
}

int Search(Position& p, Ctx& ctx, int depth, int alpha, int beta, int ply, int* bestOut) {
    ++ctx.nodes;
    if ((ctx.nodes & 1023) == 0 && CheckTime(ctx)) {
        return 0;
    }

    const int me = p.side;
    if (p.winListLen[me] > 0) {
        if (bestOut != nullptr) {
            *bestOut = p.winList[me][0];
        }
        return kWinValue - ply;
    }
    if (p.left >= 2 && p.fourListLen[me] > 0) {
        int empties[2];
        if (FourWindowEmpties(p, p.fourList[me][0], empties) == 2) {
            if (bestOut != nullptr) {
                *bestOut = empties[0];
            }
            return kWinValue - ply;
        }
    }
    if (depth <= 0) {
        return Evaluate(p, ply);
    }

    const uint64_t key = ZobristKey(p);
    TTEntry& slot = gTT[key & kTTMask];
    int ttMove = -1;
    if (slot.gen == gGen && slot.key == key) {
        ttMove = slot.move;
        if (slot.depth >= depth) {
            if (slot.flag == 0) {
                if (bestOut != nullptr && slot.move >= 0) {
                    *bestOut = slot.move;
                }
                return slot.value;
            }
            if (slot.flag == 1 && slot.value >= beta) {
                if (bestOut != nullptr && slot.move >= 0) {
                    *bestOut = slot.move;
                }
                return slot.value;
            }
            if (slot.flag == 2 && slot.value <= alpha) {
                if (bestOut != nullptr && slot.move >= 0) {
                    *bestOut = slot.move;
                }
                return slot.value;
            }
        }
    }

    int moves[kMaxMoves];
    const int n = GenerateMoves(p, moves, kMaxMoves, ttMove);
    if (n == 0) {
        return 0; // 棋盘下满
    }

    const int alphaOrig = alpha;
    int best = -kInfValue;
    int bestMove = -1;
    for (int i = 0; i < n; ++i) {
        const int m = moves[i];
        const bool flip = (p.left == 1);
        const int a2 = flip ? -beta : alpha;
        const int b2 = flip ? -alpha : beta;
        Make(p, m);
        int childBest = -1;
        int v = Search(p, ctx, depth - 1, a2, b2, ply + 1, &childBest);
        Unmake(p, m);
        if (ctx.stopped) {
            return 0;
        }
        if (flip) {
            v = -v;
        }
        if (v > best) {
            best = v;
            bestMove = m;
            if (v > alpha) {
                alpha = v;
            }
        }
        if (alpha >= beta) {
            break;
        }
    }
    if (bestMove < 0) {
        bestMove = moves[0];
    }

    TTEntry& store = gTT[key & kTTMask];
    if (store.gen != gGen || store.key != key || depth >= store.depth) {
        store.key = key;
        store.value = best;
        store.depth = static_cast<int16_t>(depth);
        store.move = static_cast<int16_t>(bestMove);
        store.gen = gGen;
        if (best <= alphaOrig) {
            store.flag = 2;
        }
        else if (best >= beta) {
            store.flag = 1;
        }
        else {
            store.flag = 0;
        }
    }
    if (bestOut != nullptr) {
        *bestOut = bestMove;
    }
    return best;
}

int PickFallbackSecond(const Position& p, int first) {
    int best = -1;
    int bestScore = -kInfValue;
    for (int i = 0; i < kCells; ++i) {
        if (p.cell[i] != 0 || i == first) {
            continue;
        }
        const int s = CellOrderScore(p, i, p.side);
        if (s > bestScore) {
            bestScore = s;
            best = i;
        }
    }
    return best;
}

} // namespace

namespace ai {

void SetTimeLimitMs(int ms) {
    gTimeLimitMs = ms > 0 ? ms : 1;
}

int GetTimeLimitMs() {
    return gTimeLimitMs;
}

void SetMaxDepth(int depth) {
    gMaxDepth = depth < 2 ? 2 : depth;
}

SearchStats GetLastStats() {
    return gLastStats;
}

} // namespace ai

std::vector<std::pair<int, int>> aiput(int tempboard[BOARDSIZE][BOARDSIZE], int side) {
    std::lock_guard<std::mutex> guard(gAiputMutex);
    GetTables(); // 初始化线表
    const int aiSide = (side == 1) ? 1 : 2;

    Position p;
    InitPosition(p, tempboard, aiSide);

    ++gGen;
    if (gGen == 0) {
        std::memset(gTT, 0, sizeof(gTT));
        gGen = 1;
    }

    Ctx ctx;
    ctx.start = std::chrono::steady_clock::now();
    ctx.timeLimitMs = gTimeLimitMs;
    ctx.maxDepth = gMaxDepth;

    const auto toPair = [](int cell) { return std::make_pair(CellX(cell), CellY(cell)); };
    std::vector<std::pair<int, int>> result;

    // ---- 1. 直接赢 / 一回合两子赢 ----
    int first = -1;
    int second = -1;
    if (p.winListLen[aiSide] > 0) {
        first = p.winList[aiSide][0];
    }
    else if (p.left >= 2 && p.fourListLen[aiSide] > 0) {
        for (int i = 0; i < p.fourListLen[aiSide]; ++i) {
            int empties[2];
            if (FourWindowEmpties(p, p.fourList[aiSide][i], empties) == 2) {
                first = empties[0];
                second = empties[1];
                break;
            }
        }
    }

    int lastDepth = 0;
    if (first < 0) {
        const bool singleStoneTurn = (p.left == 1);
        int rootMoves[kMaxMoves];
        const int nRoot = GenerateMoves(p, rootMoves, kMaxMoves, -1);
        if (nRoot == 0) {
            gLastStats.nodes = 0;
            gLastStats.depth = 0;
            gLastStats.elapsedMs = ElapsedMs(ctx);
            gLastStats.timedOut = false;
            return result; // 棋盘已满
        }

        if (singleStoneTurn) {
            int bestMove = rootMoves[0];
            int bestValue = -kInfValue;
            for (int depth = 2; depth <= ctx.maxDepth; depth += 2) {
                int iterMove = -1;
                int iterValue = -kInfValue;
                for (int i = 0; i < nRoot; ++i) {
                    const int m = rootMoves[i];
                    if (p.cell[m] != 0) {
                        continue;
                    }
                    Make(p, m);
                    int childBest = -1;
                    const int v = -Search(p, ctx, depth - 1, -kInfValue, kInfValue, 1, &childBest);
                    Unmake(p, m);
                    if (ctx.stopped) {
                        break;
                    }
                    if (v > iterValue) {
                        iterValue = v;
                        iterMove = m;
                    }
                }
                if (ctx.stopped) {
                    break;
                }
                if (iterMove >= 0) {
                    bestMove = iterMove;
                    bestValue = iterValue;
                    lastDepth = depth;
                }
                if (bestValue >= kWinValue - 1000) {
                    break;
                }
            }
            first = bestMove;
        }
        else {
            int bestFirst = -1;
            int bestSecond = -1;
            int bestValue = -kInfValue;
            for (int depth = 2; depth <= ctx.maxDepth; depth += 2) {
                int iterFirst = -1;
                int iterSecond = -1;
                int iterValue = -kInfValue;

                int ordered[kMaxMoves];
                int nOrdered = 0;
                if (bestFirst >= 0) {
                    ordered[nOrdered++] = bestFirst;
                }
                for (int i = 0; i < nRoot && nOrdered < kMaxMoves; ++i) {
                    if (rootMoves[i] != bestFirst) {
                        ordered[nOrdered++] = rootMoves[i];
                    }
                }

                for (int i = 0; i < nOrdered; ++i) {
                    const int m = ordered[i];
                    if (p.cell[m] != 0) {
                        continue;
                    }
                    Make(p, m);
                    int childBest = -1;
                    const int v = Search(p, ctx, depth - 1, -kInfValue, kInfValue, 1, &childBest);
                    Unmake(p, m);
                    if (ctx.stopped) {
                        break;
                    }
                    if (v > iterValue) {
                        iterValue = v;
                        iterFirst = m;
                        iterSecond = childBest;
                    }
                }
                if (ctx.stopped) {
                    break;
                }
                if (iterFirst >= 0) {
                    bestFirst = iterFirst;
                    bestSecond = iterSecond;
                    bestValue = iterValue;
                    lastDepth = depth;
                }
                if (bestValue >= kWinValue - 1000) {
                    break;
                }
            }
            first = bestFirst;
            second = bestSecond;
        }
    }

    if (first < 0) {
        // 兜底：随便找一个合法的
        for (int i = 0; i < kCells; ++i) {
            if (p.cell[i] == 0) {
                first = i;
                break;
            }
        }
    }

    if (first >= 0 && p.cell[first] == 0) {
        result.push_back(toPair(first));
    }
    // 黑棋开局那一回合只落一子，不能补第二手
    if (p.left != 1) {
        if (second < 0 || second == first || (second >= 0 && p.cell[second] != 0)) {
            second = PickFallbackSecond(p, first);
        }
        if (second >= 0 && second != first && p.cell[second] == 0) {
            result.push_back(toPair(second));
        }
    }

    gLastStats.nodes = ctx.nodes;
    gLastStats.depth = lastDepth;
    gLastStats.elapsedMs = ElapsedMs(ctx);
    gLastStats.timedOut = ctx.timedOut;
    return result;
}
