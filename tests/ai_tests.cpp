// Independent tactical regression tests for the new Connect-6 AI engine.
//
// The tests only use the public API:
//   std::vector<std::pair<int,int>> aiput(int board[19][19], int side);
// Every tactical assertion is made by placing the returned moves on a board
// copy and inspecting the result with a checker implemented here, so the test
// never trusts the engine's internals.
//
// Board convention: board[y][x], 0 empty, 1 black, 2 white.
// Rules: black opens with one stone, then every turn places two stones;
//        six or more in a row wins; a full board is a draw.
//
// Build & run: tests\run_ai_tests.bat   (exit code 0 = every case passed)

#include "ai/Search.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

const int N = BOARDSIZE;
using Board = int[N][N];
using Move = std::pair<int, int>;
using Moves = std::vector<Move>;

int g_cases = 0;
int g_failures = 0;

// ---------------------------------------------------------------------------
// tiny assertion helper: one Case == one PASS/FAIL line
// ---------------------------------------------------------------------------

class Case {
public:
    explicit Case(const std::string& name) : name_(name) {}

    void Expect(bool ok, const std::string& what) {
        if (!ok) {
            failures_.push_back(what);
        }
    }

    void Finish() {
        ++g_cases;
        if (failures_.empty()) {
            std::printf("PASS %s\n", name_.c_str());
            return;
        }
        ++g_failures;
        std::printf("FAIL %s\n", name_.c_str());
        for (std::size_t i = 0; i < failures_.size(); ++i) {
            std::printf("     - %s\n", failures_[i].c_str());
        }
    }

private:
    std::string name_;
    std::vector<std::string> failures_;
};

// ---------------------------------------------------------------------------
// board helpers
// ---------------------------------------------------------------------------

void Clear(Board b) { std::memset(b, 0, sizeof(Board)); }

void Put(Board b, int x, int y, int color) { b[y][x] = color; }

bool InBoard(int x, int y) { return x >= 0 && x < N && y >= 0 && y < N; }

const int kDx[4] = { 1, 0, 1, 1 };
const int kDy[4] = { 0, 1, 1, -1 };

// Calls fn(const int v[6]) once for every length-6 window along the four axes.
template <typename Fn>
void ForEachWindow(const Board b, Fn fn) {
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            for (int d = 0; d < 4; ++d) {
                const int ex = x + 5 * kDx[d];
                const int ey = y + 5 * kDy[d];
                if (!InBoard(ex, ey)) {
                    continue;
                }
                int v[6];
                for (int i = 0; i < 6; ++i) {
                    v[i] = b[y + i * kDy[d]][x + i * kDx[d]];
                }
                fn(v);
            }
        }
    }
}

// Does `color` already have six (or more) in a row anywhere?
bool HasSix(const Board b, int color) {
    bool found = false;
    ForEachWindow(b, [&](const int v[6]) {
        bool all = true;
        for (int i = 0; i < 6; ++i) {
            if (v[i] != color) {
                all = false;
                break;
            }
        }
        if (all) {
            found = true;
        }
    });
    return found;
}

// Is there a window with five own stones and one empty point (a six point)?
bool HasSixPoint(const Board b, int color) {
    bool found = false;
    ForEachWindow(b, [&](const int v[6]) {
        int own = 0;
        bool blocked = false;
        for (int i = 0; i < 6; ++i) {
            if (v[i] == color) {
                ++own;
            } else if (v[i] != 0) {
                blocked = true;
            }
        }
        if (own == 5 && !blocked) {
            found = true;
        }
    });
    return found;
}

// Is there a window with four own stones and two empty points and no opponent
// stone? Such a "four window" means this side can win with its two stones.
bool HasFourWindow(const Board b, int color) {
    bool found = false;
    ForEachWindow(b, [&](const int v[6]) {
        int own = 0;
        int other = 0;
        for (int i = 0; i < 6; ++i) {
            if (v[i] == color) {
                ++own;
            } else if (v[i] != 0) {
                ++other;
            }
        }
        if (own == 4 && other == 0) {
            found = true;
        }
    });
    return found;
}

// ---------------------------------------------------------------------------
// aiput() wrapper: validates the return contract and detects board mutation
// ---------------------------------------------------------------------------

struct AiCall {
    Moves moves;
    bool inputUnchanged = false;
    bool inRange = true;
    bool allEmpty = true;
    bool noDuplicate = true;
    std::string issue;
};

AiCall InvokeAi(const Board input, int side) {
    Board work;
    std::memcpy(work, input, sizeof(Board));
    AiCall r;
    r.moves = aiput(work, side);
    r.inputUnchanged = std::memcmp(work, input, sizeof(Board)) == 0;
    for (std::size_t i = 0; i < r.moves.size(); ++i) {
        const int x = r.moves[i].first;
        const int y = r.moves[i].second;
        if (!InBoard(x, y)) {
            r.inRange = false;
            r.issue = "a move is outside the board";
            continue;
        }
        if (input[y][x] != 0) {
            r.allEmpty = false;
            r.issue = "a move is placed on an occupied point";
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (r.moves[j] == r.moves[i]) {
                r.noDuplicate = false;
                r.issue = "the same point is returned twice";
            }
        }
    }
    return r;
}

std::string MovesStr(const Moves& moves) {
    std::string out = "[";
    for (std::size_t i = 0; i < moves.size(); ++i) {
        if (i != 0) {
            out += " ";
        }
        out += "(" + std::to_string(moves[i].first) + "," + std::to_string(moves[i].second) + ")";
    }
    out += "]";
    return out;
}

bool ContainsMove(const Moves& moves, int x, int y) {
    for (std::size_t i = 0; i < moves.size(); ++i) {
        if (moves[i].first == x && moves[i].second == y) {
            return true;
        }
    }
    return false;
}

int CountEmptyPoints(const Board b) {
    int n = 0;
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            if (b[y][x] == 0) {
                ++n;
            }
        }
    }
    return n;
}

void ApplyMoves(const Board in, const Moves& moves, int color, Board out) {
    std::memcpy(out, in, sizeof(Board));
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const int x = moves[i].first;
        const int y = moves[i].second;
        if (InBoard(x, y)) {
            out[y][x] = color;
        }
    }
}

// Common contract checks for a normal two-stone turn.
void CheckBasic(Case& c, const AiCall& r, int expectedMoves, const std::string& label) {
    c.Expect(r.inputUnchanged, label + ": aiput modified the input board");
    c.Expect(static_cast<int>(r.moves.size()) == expectedMoves,
             label + ": expected " + std::to_string(expectedMoves) + " move(s), got " +
                 std::to_string(r.moves.size()) + " " + MovesStr(r.moves));
    c.Expect(r.inRange, label + ": " + r.issue);
    c.Expect(r.allEmpty, label + ": " + r.issue);
    c.Expect(r.noDuplicate, label + ": " + r.issue);
}

// Deterministic pseudo-random midgame board with no six already on the board.
void BuildRandomMidgame(unsigned seed, int stoneCount, Board out) {
    Clear(out);
    unsigned state = seed;
    int placed = 0;
    for (int guard = 0; guard < 100000 && placed < stoneCount; ++guard) {
        state = state * 1664525u + 1013904223u;
        const int x = static_cast<int>(state % static_cast<unsigned>(N));
        state = state * 1664525u + 1013904223u;
        const int y = static_cast<int>(state % static_cast<unsigned>(N));
        if (out[y][x] != 0) {
            continue;
        }
        const int color = (placed % 2 == 0) ? 1 : 2;
        out[y][x] = color;
        if (HasSix(out, color)) {
            out[y][x] = 0;
            continue;
        }
        ++placed;
    }
}

// ---------------------------------------------------------------------------
// a) AI (2) already has five in a row with one open end: it must take the six.
// ---------------------------------------------------------------------------
void TestAWinInOne() {
    Case c("a: side 2 has five in a row, one open end -> must take the six point");
    Board b;
    Clear(b);
    for (int x = 5; x <= 9; ++x) {
        Put(b, x, 9, 2);
    }
    Put(b, 10, 9, 1);   // right end blocked, so (4,9) is the only six point
    Put(b, 2, 15, 2);   // unrelated stones
    Put(b, 14, 3, 1);
    Put(b, 15, 3, 1);

    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 2);
    CheckBasic(c, r, 2, "a");
    c.Expect(ContainsMove(r.moves, 4, 9),
             "a: winning point (4,9) not returned, got " + MovesStr(r.moves));
    Board after;
    ApplyMoves(b, r.moves, 2, after);
    c.Expect(HasSix(after, 2), "a: no six in a row for side 2 after applying the moves");
    c.Finish();
}

// ---------------------------------------------------------------------------
// b) Opponent (1) has five in a row with two open ends: both six points must be
//    occupied by the AI's two stones.
// ---------------------------------------------------------------------------
void TestBBlockBothSixPoints() {
    Case c("b: opponent has five in a row with two open ends -> must occupy both six points");
    Board b;
    Clear(b);
    for (int x = 5; x <= 9; ++x) {
        Put(b, x, 9, 1);
    }
    Put(b, 2, 15, 2);
    Put(b, 3, 15, 2);
    Put(b, 14, 3, 1);

    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 2);
    CheckBasic(c, r, 2, "b");
    const bool hasLeft = ContainsMove(r.moves, 4, 9);
    const bool hasRight = ContainsMove(r.moves, 10, 9);
    c.Expect(hasLeft && hasRight,
             "b: both (4,9) and (10,9) must be occupied, got " + MovesStr(r.moves));
    Board after;
    ApplyMoves(b, r.moves, 2, after);
    c.Expect(!HasSixPoint(after, 1), "b: opponent still has a six point after the AI turn");
    c.Finish();
}

// ---------------------------------------------------------------------------
// c) Opponent (1) has an open four (both ends empty, can make six with two
//    stones): after the AI turn no opponent four-window may survive.
// ---------------------------------------------------------------------------
void TestCBlockOpenFour() {
    Case c("c: opponent has an open four -> must close every opponent four-window");
    Board b;
    Clear(b);
    for (int x = 5; x <= 8; ++x) {
        Put(b, x, 9, 1);
    }
    Put(b, 2, 15, 2);
    Put(b, 3, 15, 2);
    Put(b, 14, 3, 1);

    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 2);
    CheckBasic(c, r, 2, "c");
    Board after;
    ApplyMoves(b, r.moves, 2, after);
    c.Expect(!HasFourWindow(after, 1),
             "c: opponent still owns a 4-stone + 2-empty window after " + MovesStr(r.moves));
    c.Expect(!HasSixPoint(after, 1),
             "c: opponent still has a six point after " + MovesStr(r.moves));
    c.Finish();
}

// ---------------------------------------------------------------------------
// d) AI (2) has a four-window: its two stones must complete six immediately.
// ---------------------------------------------------------------------------
void TestDFourToSix() {
    Case c("d: side 2 has an open four -> the two moves must complete six");
    Board b;
    Clear(b);
    for (int x = 5; x <= 8; ++x) {
        Put(b, x, 9, 2);
    }
    Put(b, 2, 15, 1);
    Put(b, 3, 15, 1);
    Put(b, 14, 3, 1);

    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 2);
    CheckBasic(c, r, 2, "d");
    Board after;
    ApplyMoves(b, r.moves, 2, after);
    c.Expect(HasSix(after, 2),
             "d: moves " + MovesStr(r.moves) + " did not complete six in a row");
    c.Finish();
}

// ---------------------------------------------------------------------------
// e) Both sides have a six point: the AI must win now instead of blocking.
// ---------------------------------------------------------------------------
void TestEWinBeforeBlock() {
    Case c("e: both sides have a six point -> the AI must win immediately");
    Board b;
    Clear(b);
    for (int x = 2; x <= 6; ++x) {
        Put(b, x, 5, 2);    // AI five, right end blocked by black
    }
    Put(b, 7, 5, 1);
    for (int x = 2; x <= 6; ++x) {
        Put(b, x, 13, 1);   // opponent five, right end blocked by AI
    }
    Put(b, 7, 13, 2);

    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 2);
    CheckBasic(c, r, 2, "e");
    c.Expect(!r.moves.empty() && r.moves[0] == Move(1, 5),
             "e: first move must be the AI six point (1,5), got " + MovesStr(r.moves));
    Moves firstOnly;
    if (!r.moves.empty()) {
        firstOnly.push_back(r.moves[0]);
    }
    Board after;
    ApplyMoves(b, firstOnly, 2, after);
    c.Expect(HasSix(after, 2), "e: the first returned move does not make six in a row");
    c.Finish();
}

// ---------------------------------------------------------------------------
// f) Random midgame position: two distinct legal empty points, board untouched.
// ---------------------------------------------------------------------------
void TestFRandomMidgame() {
    Case c("f: random midgame -> 2 distinct legal empty points, input untouched");
    Board b;
    BuildRandomMidgame(20251028u, 60, b);

    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 2);
    CheckBasic(c, r, 2, "f");
    c.Finish();
}

// Fill a board with a period-6 pattern: every run in every direction is at most
// 3 stones long, so nobody has six in a row anywhere.
void FillNoWinPattern(Board b) {
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            b[y][x] = ((x + 2 * y) % 6 < 3) ? 1 : 2;
        }
    }
}

// ---------------------------------------------------------------------------
// g) Nearly full board: no crash, no duplicate moves, only empty points.
// ---------------------------------------------------------------------------
void TestGNearlyFull() {
    {
        Case c("g1: nearly full board (2 empty points) -> 2 distinct legal moves");
        Board b;
        FillNoWinPattern(b);
        b[0][0] = 0;
        b[18][18] = 0;
        ai::SetTimeLimitMs(100);
        const AiCall r = InvokeAi(b, 2);
        CheckBasic(c, r, 2, "g1");
        Board after;
        ApplyMoves(b, r.moves, 2, after);
        c.Finish();
    }
    {
        Case c("g2: nearly full board (1 empty point) -> 1..2 legal moves, no duplicates");
        Board b;
        FillNoWinPattern(b);
        b[9][9] = 0;
        ai::SetTimeLimitMs(100);
        const AiCall r = InvokeAi(b, 2);
        c.Expect(r.inputUnchanged, "g2: aiput modified the input board");
        c.Expect(!r.moves.empty(), "g2: aiput returned no move although one empty point exists");
        c.Expect(r.moves.size() <= 2, "g2: more than 2 moves returned");
        c.Expect(r.inRange, "g2: " + r.issue);
        c.Expect(r.allEmpty, "g2: " + r.issue);
        c.Expect(r.noDuplicate, "g2: " + r.issue);
        for (std::size_t i = 0; i < r.moves.size(); ++i) {
            c.Expect(r.moves[i] == Move(9, 9),
                     "g2: move " + std::to_string(i) + " is not the only empty point (9,9)");
        }
        c.Finish();
    }
}

// Quiet midgame board: a filled rectangle of the no-win pattern, the rest of
// the board empty. Windows fully inside the rectangle are either 3-vs-3 or
// 4-vs-2, and border windows contain empties, so neither side owns a four
// window or a five/six point: the AI cannot shortcut with an immediate win or
// an immediate defence and has to run its real search until the time limit.
void BuildQuietRectangle(int x0, int y0, int w, int h, Board out) {
    Clear(out);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h) {
                out[y][x] = ((x + 2 * y) % 6 < 3) ? 1 : 2;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// h) Timing: with SetTimeLimitMs(200) every single call must stay well below
//    1500 ms (the limit is a target, not a hard wall, so allow some slack).
//    Quiet midgame positions force the real iterative search, not a shortcut.
// ---------------------------------------------------------------------------
void TestHTiming() {
    Case c("h: SetTimeLimitMs(200) -> every call below 1500 ms");
    ai::SetTimeLimitMs(200);
    const int origins[3][2] = { { 0, 0 }, { 7, 7 }, { 4, 4 } };
    for (int i = 0; i < 3; ++i) {
        Board b;
        BuildQuietRectangle(origins[i][0], origins[i][1], 12, 12, b);
        const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        const AiCall r = InvokeAi(b, 2);
        const std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
        const long long ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        const ai::SearchStats stats = ai::GetLastStats();
        std::printf("     h[%d]: origin=(%d,%d) empties=%d time_limit=%d ms elapsed=%lld ms "
                    "depth=%d nodes=%lld timedOut=%d\n",
                    i, origins[i][0], origins[i][1], CountEmptyPoints(b), ai::GetTimeLimitMs(),
                    ms, stats.depth, stats.nodes, stats.timedOut ? 1 : 0);
        CheckBasic(c, r, 2, "h");
        c.Expect(ms < 1500,
                 "h: call " + std::to_string(i) + " took " + std::to_string(ms) +
                     " ms (>= 1500 ms)");
    }
    c.Finish();
    ai::SetTimeLimitMs(500);
}

// ---------------------------------------------------------------------------
// i) Empty board, side 1 (black): black opens with exactly one stone.
// ---------------------------------------------------------------------------
void TestIEmptyBoardBlackOpening() {
    Case c("i: empty board, side 1 (black) -> exactly one stone (black opening)");
    Board b;
    Clear(b);
    ai::SetTimeLimitMs(100);
    const AiCall r = InvokeAi(b, 1);
    c.Expect(r.inputUnchanged, "i: aiput modified the input board");
    c.Expect(r.moves.size() == 1,
             "i: expected exactly 1 opening stone, got " + std::to_string(r.moves.size()) +
                 " " + MovesStr(r.moves));
    c.Expect(r.inRange, "i: " + r.issue);
    c.Expect(r.allEmpty, "i: " + r.issue);
    c.Expect(r.noDuplicate, "i: " + r.issue);
    c.Finish();
}

} // namespace

int main() {
    std::printf("=== Connect-6 AI tactical tests (new engine) ===\n");

    TestAWinInOne();
    TestBBlockBothSixPoints();
    TestCBlockOpenFour();
    TestDFourToSix();
    TestEWinBeforeBlock();
    TestFRandomMidgame();
    TestGNearlyFull();
    TestHTiming();
    TestIEmptyBoardBlackOpening();

    std::printf("\n%d case(s) run, %d failed\n", g_cases, g_failures);
    if (g_failures == 0) {
        std::printf("ALL AI TESTS PASSED\n");
        return 0;
    }
    std::printf("%d AI TEST CASE(S) FAILED\n", g_failures);
    return 1;
}
