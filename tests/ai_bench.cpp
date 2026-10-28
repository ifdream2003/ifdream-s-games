// Self-play benchmark: new AI engine (aiput) versus the legacy AI snapshot
// (aiput_legacy).
//
// Rules used here:
//   * black (1) opens with a single stone, every later turn places two stones;
//   * six or more in a row wins, a full board is a draw;
//   * the AI's own move list is validated before it is applied: a move outside
//     the board, on an occupied point or duplicated inside the turn aborts the
//     current game and is reported as an illegal move.
//
// The new engine gets ai::SetTimeLimitMs(100) for every call (turn).
//
// Usage:
//   ai_bench.exe [number_of_games]     (default 4)
//
// Build & run: tests\run_ai_bench.bat [number_of_games]

#include "ai/LegacyAi.h"
#include "ai/Search.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

const int N = BOARDSIZE;
using Board = int[N][N];
using Move = std::pair<int, int>;
using Moves = std::vector<Move>;

const int kDx[4] = { 1, 0, 1, 1 };
const int kDy[4] = { 0, 1, 1, -1 };

bool InBoard(int x, int y) { return x >= 0 && x < N && y >= 0 && y < N; }

// Independent six-in-a-row check used to decide the winner.
bool HasSix(const Board b, int color) {
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            if (b[y][x] != color) {
                continue;
            }
            for (int d = 0; d < 4; ++d) {
                int count = 1;
                int cx = x + kDx[d];
                int cy = y + kDy[d];
                while (InBoard(cx, cy) && b[cy][cx] == color) {
                    ++count;
                    cx += kDx[d];
                    cy += kDy[d];
                }
                if (count >= 6) {
                    return true;
                }
            }
        }
    }
    return false;
}

int CountEmpty(const Board b) {
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

struct GameStats {
    int game = 0;                 // 0-based game index
    int newColor = 1;             // 1 = black, 2 = white
    std::string result;
    bool newWon = false;
    bool legacyWon = false;
    bool draw = false;
    bool aborted = false;
    int stones = 0;               // stones placed in total
    int turns = 0;                // aiput() calls in total (both engines)
    long long newTotalUs = 0;     // summed new-engine thinking time
    int newCalls = 0;
    long long wallMs = 0;
    std::string error;
};

struct Totals {
    int games = 0;
    int newWins = 0;
    int legacyWins = 0;
    int draws = 0;
    int aborted = 0;
    long long newTotalUs = 0;
    int newCalls = 0;
    long long wallMs = 0;
};

const Moves& AskEngine(bool newEngine, const Board board, int side, Moves* storage) {
    Board work;
    std::memcpy(work, board, sizeof(Board));
    if (newEngine) {
        *storage = aiput(work, side);
    } else {
        *storage = aiput_legacy(work, side);
    }
    return *storage;
}

void PlayGame(int gameNo, GameStats& st) {
    Board b;
    std::memset(b, 0, sizeof(Board));

    st.game = gameNo;
    st.newColor = (gameNo % 2 == 0) ? 1 : 2; // alternate colours every game
    st.result = "UNKNOWN";

    int side = 1; // black always moves first
    int turn = 0;
    const std::chrono::steady_clock::time_point wallStart =
        std::chrono::steady_clock::now();

    while (true) {
        const int empties = CountEmpty(b);
        if (empties == 0) {
            st.draw = true;
            st.result = "DRAW (board full)";
            break;
        }

        const int want = (turn == 0) ? 1 : 2; // black opens with one stone
        const int take = (want < empties) ? want : empties;
        const bool isNew = (side == st.newColor);
        ++st.turns;

        Moves moves;
        if (isNew) {
            const std::chrono::steady_clock::time_point t0 =
                std::chrono::steady_clock::now();
            AskEngine(true, b, side, &moves);
            const std::chrono::steady_clock::time_point t1 =
                std::chrono::steady_clock::now();
            st.newTotalUs +=
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
            ++st.newCalls;
        } else {
            AskEngine(false, b, side, &moves);
        }

        bool illegal = false;
        for (int i = 0; i < take; ++i) {
            const std::string who = isNew ? "new engine" : "legacy engine";
            if (i >= static_cast<int>(moves.size())) {
                st.error = who + " returned only " + std::to_string(moves.size()) +
                           " move(s) but the turn needs " + std::to_string(take);
                illegal = true;
                break;
            }
            const int x = moves[static_cast<std::size_t>(i)].first;
            const int y = moves[static_cast<std::size_t>(i)].second;
            if (!InBoard(x, y)) {
                st.error = who + " returned an out-of-board move (" + std::to_string(x) + "," +
                           std::to_string(y) + ")";
                illegal = true;
                break;
            }
            if (b[y][x] != 0) {
                st.error = who + " returned an occupied or duplicated point (" +
                           std::to_string(x) + "," + std::to_string(y) + ")";
                illegal = true;
                break;
            }
            b[y][x] = side;
            ++st.stones;
            if (HasSix(b, side)) {
                if (isNew) {
                    st.newWon = true;
                } else {
                    st.legacyWon = true;
                }
                break;
            }
            if (CountEmpty(b) == 0) {
                st.draw = true;
                break;
            }
        }

        if (illegal) {
            st.aborted = true;
            st.result = "ABORTED (illegal move)";
            break;
        }
        if (st.newWon) {
            st.result = "NEW_WIN";
            break;
        }
        if (st.legacyWon) {
            st.result = "LEGACY_WIN";
            break;
        }
        if (st.draw) {
            st.result = "DRAW (board full)";
            break;
        }

        side = 3 - side;
        ++turn;
    }

    const std::chrono::steady_clock::time_point wallEnd = std::chrono::steady_clock::now();
    st.wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(wallEnd - wallStart).count();
}

void PrintGame(const GameStats& st) {
    const char* blackEngine = (st.newColor == 1) ? "new" : "legacy";
    const char* whiteEngine = (st.newColor == 1) ? "legacy" : "new";
    const double newAvgMs = (st.newCalls > 0)
                                ? (static_cast<double>(st.newTotalUs) / st.newCalls / 1000.0)
                                : 0.0;
    std::printf("Game %d: black=%s white=%s -> %-20s | stones=%3d turns=%3d | "
                "new avg %7.1f ms/turn (%d calls) | wall %.1f s",
                st.game + 1, blackEngine, whiteEngine, st.result.c_str(), st.stones, st.turns,
                newAvgMs, st.newCalls, static_cast<double>(st.wallMs) / 1000.0);
    if (st.aborted) {
        std::printf(" | ERROR: %s", st.error.c_str());
    }
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    int games = 4;
    if (argc > 1) {
        games = std::atoi(argv[1]);
        if (games < 1) {
            games = 1;
        }
        if (games > 40) {
            games = 40;
        }
    }

    ai::SetTimeLimitMs(100);

    std::printf("=== Connect-6 self-play benchmark: new engine vs legacy engine ===\n");
    std::printf("games=%d  new-engine time limit=%d ms/turn  "
                "(black opens with 1 stone, then 2 stones per turn)\n",
                games, ai::GetTimeLimitMs());
    std::fflush(stdout);

    Totals totals;
    for (int g = 0; g < games; ++g) {
        GameStats st;
        PlayGame(g, st);
        PrintGame(st);
        std::fflush(stdout);

        ++totals.games;
        totals.newWins += st.newWon ? 1 : 0;
        totals.legacyWins += st.legacyWon ? 1 : 0;
        totals.draws += st.draw ? 1 : 0;
        totals.aborted += st.aborted ? 1 : 0;
        totals.newTotalUs += st.newTotalUs;
        totals.newCalls += st.newCalls;
        totals.wallMs += st.wallMs;
    }

    const double overallAvgMs =
        (totals.newCalls > 0)
            ? (static_cast<double>(totals.newTotalUs) / totals.newCalls / 1000.0)
            : 0.0;

    std::printf("\n--- benchmark summary ---\n");
    std::printf("games=%d  new-engine wins=%d  legacy wins=%d  draws=%d  aborted=%d\n",
                totals.games, totals.newWins, totals.legacyWins, totals.draws, totals.aborted);
    std::printf("new engine average %.1f ms/turn over %d calls\n", overallAvgMs,
                totals.newCalls);
    std::printf("total wall time %.1f s (%.1f s/game)\n",
                static_cast<double>(totals.wallMs) / 1000.0,
                (totals.games > 0) ? static_cast<double>(totals.wallMs) / totals.games / 1000.0
                                   : 0.0);
    std::printf("new engine wins %d, legacy wins %d, draws %d\n", totals.newWins,
                totals.legacyWins, totals.draws);
    std::printf("\u6c47\u603b: \u65b0\u5f15\u64ce\u80dc %d \u65e7\u5f15\u64ce\u80dc %d "
                "\u548c %d\n",
                totals.newWins, totals.legacyWins, totals.draws);

    if (totals.aborted > 0) {
        std::printf("\nWARNING: %d game(s) aborted because of illegal engine moves\n",
                    totals.aborted);
        return 1;
    }
    return 0;
}
