// 旧版 AI 的原始实现快照（原 ai/Evaluator.cpp + ai/Search.cpp）。
// 保留它是为了在测试和基准里做“新引擎 vs 旧算法”的自我对弈对比。
#include "ai/LegacyAi.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/GameState.h"
#include "core/Rules.h"

using namespace std;

namespace {

const int kLegacyTopN = 5; // 原 TOP_N
const int kLegacyDepth = 3; // 原 dep

struct LegacyPatternScore {
    const char* text;
    int score;
};

struct LegacyScoredMove {
    int score;
    int x;
    int y;
};

// AI 视角的棋形评分表
const LegacyPatternScore kLegacyAiPatterns[] = {
    {"222222", 200000}, {"22222", 90000}, {"202222", 40000}, {"222022", 40000},
    {"220222", 40000},  {"222202", 40000}, {"2222", 30000},  {"20222", 20000},
    {"22202", 20000},   {"22022", 20000},  {"222002", 10000}, {"220022", 10000},
    {"200222", 10000},  {"222", 500},      {"22", 100},      {"2111", 600},
    {"211", 150},       {"211111", 50000}, {"21111", 45000},
};
const int kLegacyAiCount = static_cast<int>(sizeof(kLegacyAiPatterns) / sizeof(kLegacyAiPatterns[0]));

// 玩家视角的棋形评分表
const LegacyPatternScore kLegacyPlayerPatterns[] = {
    {"111111", -200000}, {"11111", -90000},  {"101111", -40000}, {"111011", -40000},
    {"110111", -40000},  {"111101", -40000}, {"1111", -30000},   {"10111", -20000},
    {"11101", -20000},   {"11011", -20000},  {"111001", -10000}, {"110011", -10000},
    {"100111", -10000},  {"111", -500},      {"11", -100},       {"1222", -650},
    {"122", -150},       {"122222", -50000}, {"12222", -45000},
};
const int kLegacyPlayerCount =
    static_cast<int>(sizeof(kLegacyPlayerPatterns) / sizeof(kLegacyPlayerPatterns[0]));

const int kLegacyDirs[8][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, 1}, {1, -1}, {-1, 1}
};

void LegacyBuildChessLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE], char lines[8][8]) {
    const int range = 7;
    for (int d = 0; d < 8; ++d) {
        int cx = x;
        int cy = y;
        int n = 0;
        while (n < range && cx >= 0 && cx < BOARDSIZE && cy >= 0 && cy < BOARDSIZE) {
            lines[d][n++] = static_cast<char>('0' + tempboard[cy][cx]);
            cx += kLegacyDirs[d][0];
            cy += kLegacyDirs[d][1];
        }
        lines[d][n] = '\0';
    }
}

void LegacyNormalizePattern(const char* line, char* out) {
    const int len = static_cast<int>(strlen(line));
    const char base = line[0];
    const char opponent = (base == '2') ? '1' : '2';
    int n = 0;
    int i = 1;
    out[n++] = base;
    if (len > 1) {
        if (line[1] == base) {
            while (i < len && line[i] == base) {
                out[n++] = line[i++];
            }
            if (i < len && line[i] == '0') {
                while (i < len && line[i] == '0') {
                    out[n++] = line[i++];
                }
                if (i < len && line[i] == base) {
                    while (i < len && line[i] == base) {
                        out[n++] = line[i++];
                    }
                }
            }
        }
        else {
            if (line[i] == '0') {
                while (i < len && line[i] == '0') {
                    out[n++] = line[i++];
                }
                if (i < len && line[i] == base) {
                    while (i < len && line[i] == base) {
                        out[n++] = line[i++];
                    }
                }
            }
            else if (line[i] == opponent) {
                while (i < len && line[i] == opponent) {
                    out[n++] = line[i++];
                }
            }
        }
    }
    out[n] = '\0';
}

int LegacyScoreCell(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE],
                    const LegacyPatternScore* patterns, int patternCount, bool* foundPattern) {
    char lines[8][8];
    LegacyBuildChessLines(x, y, tempboard, lines);
    int total = 0;
    *foundPattern = false;
    for (int d = 0; d < 8; ++d) {
        char normalized[8];
        LegacyNormalizePattern(lines[d], normalized);
        for (int p = 0; p < patternCount; ++p) {
            if (strstr(normalized, patterns[p].text) != nullptr) {
                total += patterns[p].score;
                *foundPattern = true;
            }
        }
    }
    return total;
}

void LegacyMarkCandidates(int tempboard[BOARDSIZE][BOARDSIZE], bool candidates[BOARDSIZE][BOARDSIZE]) {
    memset(candidates, 0, sizeof(bool) * BOARDSIZE * BOARDSIZE);
    bool hasStone = false;
    for (int y = 0; y < BOARDSIZE; ++y) {
        for (int x = 0; x < BOARDSIZE; ++x) {
            if (tempboard[y][x] == 0) {
                continue;
            }
            hasStone = true;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && nx < BOARDSIZE && ny >= 0 && ny < BOARDSIZE) {
                        candidates[ny][nx] = true;
                    }
                }
            }
        }
    }
    if (!hasStone) {
        for (int y = 0; y < BOARDSIZE; ++y) {
            for (int x = 0; x < BOARDSIZE; ++x) {
                candidates[y][x] = true;
            }
        }
    }
}

void LegacyPushBest(LegacyScoredMove best[kLegacyTopN], int* count, int score, int x, int y,
                    bool keepHigh) {
    int pos = *count;
    if (*count < kLegacyTopN) {
        ++(*count);
    }
    else {
        pos = 0;
        for (int i = 1; i < kLegacyTopN; ++i) {
            if (keepHigh ? best[i].score < best[pos].score : best[i].score > best[pos].score) {
                pos = i;
            }
        }
        if (keepHigh ? score <= best[pos].score : score >= best[pos].score) {
            return;
        }
    }
    best[pos].score = score;
    best[pos].x = x;
    best[pos].y = y;
}

multimap<int, pair<int, int>> LegacyEvaluateMoves(int tempboard[][BOARDSIZE],
                                                  const LegacyPatternScore* patterns,
                                                  int patternCount, int stone, bool keepHigh,
                                                  bool subtractCenter) {
    bool candidates[BOARDSIZE][BOARDSIZE];
    LegacyMarkCandidates(tempboard, candidates);

    const int center = BOARDSIZE / 2;
    LegacyScoredMove best[kLegacyTopN];
    int count = 0;

    for (int y = 0; y < BOARDSIZE; ++y) {
        for (int x = 0; x < BOARDSIZE; ++x) {
            if (!candidates[y][x] || !isValid(x, y, tempboard)) {
                continue;
            }
            tempboard[y][x] = stone;
            bool foundPattern = false;
            int totalScore = LegacyScoreCell(x, y, tempboard, patterns, patternCount, &foundPattern);
            tempboard[y][x] = 0;

            if (!foundPattern) {
                const int centerScore = BOARDSIZE - (abs(center - y) + abs(center - x));
                totalScore += subtractCenter ? -centerScore : centerScore;
            }
            LegacyPushBest(best, &count, totalScore, x, y, keepHigh);
        }
    }

    multimap<int, pair<int, int>> result;
    for (int i = 0; i < count; ++i) {
        result.insert({ best[i].score, { best[i].x, best[i].y } });
    }
    return result;
}

multimap<int, pair<int, int>> LegacyAiMove(int tempboard[][BOARDSIZE], int step) {
    LegacyPatternScore patterns[kLegacyAiCount];
    memcpy(patterns, kLegacyAiPatterns, sizeof(patterns));
    if (step == 1) {
        patterns[kLegacyAiCount - 1] = { "211111", 65000 };
        patterns[kLegacyAiCount - 2] = { "21111", 60000 };
    }
    return LegacyEvaluateMoves(tempboard, patterns, kLegacyAiCount, 2, true, false);
}

multimap<int, pair<int, int>> LegacyPlayerMove(int tempboard[][BOARDSIZE], int step) {
    LegacyPatternScore patterns[kLegacyPlayerCount];
    memcpy(patterns, kLegacyPlayerPatterns, sizeof(patterns));
    if (step == 3) {
        patterns[kLegacyPlayerCount - 1] = { "122222", -65000 };
        patterns[kLegacyPlayerCount - 2] = { "12222", -60000 };
    }
    return LegacyEvaluateMoves(tempboard, patterns, kLegacyPlayerCount, 1, false, true);
}

multimap<int, pair<int, int>> LegacyScoreTopTen(int boardtemp[BOARDSIZE][BOARDSIZE], int step,
                                                int depth) {
    int simboard[BOARDSIZE][BOARDSIZE];
    copyBoard(boardtemp, simboard);

    multimap<int, pair<int, int>> currentMoves;
    vector<multimap<int, pair<int, int>>> candidates;
    multimap<int, pair<int, int>> topNMoves;

    if (depth == 0) {
        if (step == 0 || step == 1) {
            return LegacyAiMove(simboard, step);
        }
        return LegacyPlayerMove(simboard, step);
    }

    if (step == 0) {
        currentMoves = LegacyAiMove(simboard, step);
        int i = 0;
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {
            simboard[it->second.second][it->second.first] = 2;
            candidates.push_back(LegacyScoreTopTen(simboard, (step + 1) % 4, depth - 1));
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {
            const int score = it->first + candidates[i++].rbegin()->first;
            topNMoves.insert({ score, it->second });
        }
    }
    if (step == 1) {
        currentMoves = LegacyAiMove(simboard, step);
        int i = 0;
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {
            simboard[it->second.second][it->second.first] = 2;
            candidates.push_back(LegacyScoreTopTen(simboard, (step + 1) % 4, depth - 1));
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.rbegin(); it != currentMoves.rend(); it++) {
            const int score = it->first + static_cast<int>(candidates[i++].begin()->first * 0.8);
            topNMoves.insert({ score, it->second });
        }
    }
    if (step == 2) {
        currentMoves = LegacyPlayerMove(simboard, step);
        int i = 0;
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {
            simboard[it->second.second][it->second.first] = 1;
            candidates.push_back(LegacyScoreTopTen(simboard, (step + 1) % 4, depth - 1));
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {
            const int score = it->first + candidates[i++].begin()->first;
            topNMoves.insert({ score, it->second });
        }
    }
    if (step == 3) {
        currentMoves = LegacyPlayerMove(simboard, step);
        int i = 0;
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {
            simboard[it->second.second][it->second.first] = 1;
            candidates.push_back(LegacyScoreTopTen(simboard, (step + 1) % 4, depth - 1));
            simboard[it->second.second][it->second.first] = 0;
            i++;
        }
        i = 0;
        for (auto it = currentMoves.begin(); it != currentMoves.end(); it++) {
            const int score = it->first + static_cast<int>(candidates[i++].rbegin()->first * 0.8);
            topNMoves.insert({ score, it->second });
        }
    }
    return topNMoves;
}

vector<pair<int, int>> LegacyAiput(int tempboard[BOARDSIZE][BOARDSIZE]) {
    vector<pair<int, int>> moves;
    auto firstMoves = LegacyScoreTopTen(tempboard, 0, kLegacyDepth);
    if (firstMoves.empty()) {
        return moves;
    }
    auto firstMove = firstMoves.rbegin()->second;
    moves.push_back(firstMove);
    tempboard[firstMove.second][firstMove.first] = 2;
    auto secondMoves = LegacyScoreTopTen(tempboard, 1, kLegacyDepth);
    tempboard[firstMove.second][firstMove.first] = 0;
    if (!secondMoves.empty()) {
        moves.push_back(secondMoves.rbegin()->second);
    }
    return moves;
}

} // namespace

std::vector<std::pair<int, int>> aiput_legacy(int tempboard[BOARDSIZE][BOARDSIZE], int side) {
    if (side == 2) {
        return LegacyAiput(tempboard);
    }
    // 旧版把“2”当 AI：交换黑白后计算，再把结果照原样返回（坐标不变）
    int swapped[BOARDSIZE][BOARDSIZE];
    for (int y = 0; y < BOARDSIZE; ++y) {
        for (int x = 0; x < BOARDSIZE; ++x) {
            const int v = tempboard[y][x];
            swapped[y][x] = (v == 1) ? 2 : ((v == 2) ? 1 : 0);
        }
    }
    return LegacyAiput(swapped);
}
