#include "ai/Evaluator.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "core/GameState.h"
#include "core/Rules.h"

using namespace std;

namespace {

struct PatternScore {
    const char* text;
    int score;
};

struct ScoredMove {
    int score;
    int x;
    int y;
};

// AI 视角的棋形评分表
const PatternScore kAiPatterns[] = {
    // AI的强进攻棋形
    {"222222", 200000}, // AI六连
    {"22222", 90000},   // AI五连
    {"202222", 40000},
    {"222022", 40000},
    {"220222", 40000},
    {"222202", 40000},
    {"2222", 30000},
    {"20222", 20000},
    {"22202", 20000},
    {"22022", 20000},
    {"222002", 10000},
    {"220022", 10000},
    {"200222", 10000},

    // AI的弱进攻棋形
    {"222", 500}, // AI活三
    {"22", 100},  // AI活二

    // ai防守棋形
    {"2111", 600},      // 阻挡玩家活三
    {"211", 150},       // 阻挡玩家活二
    {"211111", 50000},  // 阻挡玩家五连
    {"21111", 45000},   // 阻挡玩家冲四
};
const int kAiPatternCount = static_cast<int>(sizeof(kAiPatterns) / sizeof(kAiPatterns[0]));

// 玩家视角的棋形评分表（负分越小越危险）
const PatternScore kPlayerPatterns[] = {
    // 【中优先级】玩家自身进攻（优先级低于防守AI高危）
    {"111111", -200000}, // 玩家六连（必胜）
    {"11111", -90000},   // 玩家五连（下一步必胜）

    // 玩家的绝对优势棋形
    {"101111", -40000}, // 玩家活四
    {"111011", -40000},
    {"110111", -40000},
    {"111101", -40000},

    // 玩家的强进攻棋形
    {"1111", -30000},  // 玩家四连
    {"10111", -20000}, // 玩家冲四
    {"11101", -20000},
    {"11011", -20000},

    // 玩家的中等进攻棋形
    {"111001", -10000}, // 玩家眠四
    {"110011", -10000},
    {"100111", -10000},

    // 玩家的弱进攻棋形
    {"111", -500}, // 玩家活三
    {"11", -100},  // 玩家活二

    // --- 【AI防守棋形】---
    {"1222", -650},       // 阻挡AI活三
    {"122", -150},        // 阻挡AI活二
    {"122222", -50000},   // 阻挡AI五连
    {"12222", -45000},    // 阻挡AI冲四
};
const int kPlayerPatternCount = static_cast<int>(sizeof(kPlayerPatterns) / sizeof(kPlayerPatterns[0]));

// 8 条线的方向：左、右、上、下、左上、右下、右上、左下（与 core::getChessLines 顺序一致）
const int kDirs[8][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1},
    {-1, -1}, {1, 1}, {1, -1}, {-1, 1}
};

// 与 core::getChessLines 等价：从 (x,y) 沿 8 个方向各取最多 7 格（含自身），
// 改用固定缓冲区，避免每个候选点都分配 std::string
void BuildChessLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE], char lines[8][8]) {
    const int range = 7;
    for (int d = 0; d < 8; ++d) {
        int cx = x;
        int cy = y;
        int n = 0;
        while (n < range && cx >= 0 && cx < BOARDSIZE && cy >= 0 && cy < BOARDSIZE) {
            lines[d][n++] = static_cast<char>('0' + tempboard[cy][cx]);
            cx += kDirs[d][0];
            cy += kDirs[d][1];
        }
        lines[d][n] = '\0';
    }
}

// 与 core::getPattern 等价的棋形归一化，只是写进固定缓冲区（最多 7 个字符）
void NormalizePattern(const char* line, char* out) {
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
        } else {
            if (line[i] == '0') {
                while (i < len && line[i] == '0') {
                    out[n++] = line[i++];
                }
                if (i < len && line[i] == base) {
                    while (i < len && line[i] == base) {
                        out[n++] = line[i++];
                    }
                }
            } else if (line[i] == opponent) {
                while (i < len && line[i] == opponent) {
                    out[n++] = line[i++];
                }
            }
        }
    }
    out[n] = '\0';
}

// 复刻原实现的打分：8 条线归一化后，用棋形串做子串匹配并累加分数
int ScoreCell(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE],
              const PatternScore* patterns, int patternCount, bool* foundPattern) {
    char lines[8][8];
    BuildChessLines(x, y, tempboard, lines);
    int total = 0;
    *foundPattern = false;
    for (int d = 0; d < 8; ++d) {
        char normalized[8];
        NormalizePattern(lines[d], normalized);
        for (int p = 0; p < patternCount; ++p) {
            if (strstr(normalized, patterns[p].text) != nullptr) {
                total += patterns[p].score;
                *foundPattern = true;
            }
        }
    }
    return total;
}

// 只评估已有棋子周围 2 格内的空位：远离战场的点没有棋形价值，
// 这是把候选点从 361 个压下来的关键优化
void MarkCandidates(int tempboard[BOARDSIZE][BOARDSIZE], bool candidates[BOARDSIZE][BOARDSIZE]) {
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

// 维护前 TOP_N 个候选点（keepHigh 为真取最高分，否则取最低分）
void PushBest(ScoredMove best[TOP_N], int* count, int score, int x, int y, bool keepHigh) {
    int pos = *count;
    if (*count < TOP_N) {
        ++(*count);
    } else {
        pos = 0;
        for (int i = 1; i < TOP_N; ++i) {
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

multimap<int, pair<int, int>> EvaluateMoves(int tempboard[][BOARDSIZE],
                                            const PatternScore* patterns, int patternCount,
                                            int stone, bool keepHigh, bool subtractCenter) {
    bool candidates[BOARDSIZE][BOARDSIZE];
    MarkCandidates(tempboard, candidates);

    const int center = BOARDSIZE / 2;
    ScoredMove best[TOP_N];
    int count = 0;

    for (int y = 0; y < BOARDSIZE; ++y) { // 注意：y 是行，x 是列
        for (int x = 0; x < BOARDSIZE; ++x) {
            if (!candidates[y][x] || !isValid(x, y, tempboard)) {
                continue;
            }
            // 模拟落子
            tempboard[y][x] = stone;
            bool foundPattern = false;
            int totalScore = ScoreCell(x, y, tempboard, patterns, patternCount, &foundPattern);
            // 撤销模拟落子
            tempboard[y][x] = 0;

            if (!foundPattern) {
                const int centerScore = BOARDSIZE - (abs(center - y) + abs(center - x));
                totalScore += subtractCenter ? -centerScore : centerScore;
            }
            PushBest(best, &count, totalScore, x, y, keepHigh);
        }
    }

    multimap<int, pair<int, int>> result;
    for (int i = 0; i < count; ++i) {
        result.insert({ best[i].score, { best[i].x, best[i].y } });
    }
    return result;
}

} // namespace

// ai落子评估函数
multimap<int, pair<int, int>> aiMove(int tempboard[][BOARDSIZE], int step) {
    PatternScore patterns[kAiPatternCount];
    memcpy(patterns, kAiPatterns, sizeof(patterns));
    if (step == 1) {
        patterns[kAiPatternCount - 1] = { "211111", 65000 };
        patterns[kAiPatternCount - 2] = { "21111", 60000 };
    }
    return EvaluateMoves(tempboard, patterns, kAiPatternCount, 2, true, false);
}

// 玩家落子评估函数
multimap<int, pair<int, int>> playerMove(int tempboard[][BOARDSIZE], int step) {
    PatternScore patterns[kPlayerPatternCount];
    memcpy(patterns, kPlayerPatterns, sizeof(patterns));
    if (step == 3) {
        patterns[kPlayerPatternCount - 1] = { "122222", -65000 };
        patterns[kPlayerPatternCount - 2] = { "12222", -60000 };
    }
    return EvaluateMoves(tempboard, patterns, kPlayerPatternCount, 1, false, true);
}
