#include "ai/Evaluator.h"

#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/GameState.h"
#include "core/Rules.h"

using namespace std;

//ai落子评估函数
multimap<int, pair<int, int>> aiMove(int tempboard[][BOARDSIZE], int step) {

    vector<pair<string, int>> bestMove = {
        // AI的强进攻棋形
        {"222222",200000}, // AI六连
        {"22222", 90000},  // AI五连
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
        {"222", 500},     // AI活三
        {"22", 100},      // AI活二

		//ai防守棋形
        {"2111", 600},   // 阻挡玩家活三
        {"211", 150},     // 阻挡玩家活二
        {"211111", 50000 }, // 阻挡玩家五连
        {"21111", 45000},  // 阻挡玩家冲四
    };

    int i = bestMove.size();
    if (step == 1) {
        bestMove[i - 1] = { "211111",65000 };
        bestMove[i - 2] = { "21111",60000 };
    }
    // 棋盘中心权重（初期布局优先中心）
    int center = BOARDSIZE / 2;
    int scoreBoard[BOARDSIZE][BOARDSIZE] = { 0 };
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            scoreBoard[i][j] = BOARDSIZE - (abs(center - i) + abs(center - j));
        }
    }

    multimap<int, pair<int, int>> topTenScore; // 存储Top10高分落子点
    // 遍历所有空位，评估落子价值
    for (int y = 0; y < BOARDSIZE; y++) { // 注意：y 是行，x 是列
        for (int x = 0; x < BOARDSIZE; x++) {
            if (isValid(x, y)) {
                // 模拟 AI 落子 (注意是 tempboard[y][x])
                tempboard[y][x] = 2;
                vector<string> lines = getChessLines(x, y, tempboard);
                int totalScore = 0;
                bool foundPattern = false;

                // 计算当前落子位置的分数
                for (auto line : lines) {
                    for (auto pattern : bestMove) {
                        if (line.find(pattern.first) != string::npos) {
                            totalScore += pattern.second;
                            foundPattern = true;
                        }
                    }
                }
                if (!foundPattern) {
                    totalScore += scoreBoard[y][x]; // 注意是 scoreBoard[y][x]
                }

                // 将当前落子位置和分数加入分数容器
                if (topTenScore.size() < TOP_N) {
                    topTenScore.insert({ totalScore,{x,y} });
                }
                else if (totalScore > topTenScore.begin()->first) {
                    topTenScore.erase(topTenScore.begin());
                    topTenScore.insert({ totalScore,{x,y} });
                }

                // 撤销模拟落子 (注意是 board[y][x])
                tempboard[y][x] = 0;
            }
        }
    }

    return topTenScore;
}

//玩家落子评估函数
multimap<int, pair<int, int>> playerMove(int tempboard[][BOARDSIZE], int step) {
    // 评分体系：玩家视角，分数为负值，1和2交换
    vector<pair<string, int>> bestMove = {
        // 【中优先级】玩家自身进攻（优先级低于防守AI高危）
        {"111111", -200000}, // 玩家六连（必胜）
        {"11111", -90000},  // 玩家五连（下一步必胜）

        // 玩家的绝对优势棋形
        {"101111", -40000}, // 玩家活四
        {"111011", -40000},
        {"110111", -40000},
        {"111101", -40000},

        // 玩家的强进攻棋形
        {"1111",  -30000},   // 玩家四连
        {"10111", -20000},  // 玩家冲四
        {"11101", -20000},
        {"11011", -20000},

        // 玩家的中等进攻棋形
        {"111001", -10000}, // 玩家眠四
        {"110011", -10000},
        {"100111", -10000},

        // 玩家的弱进攻棋形
        {"111", -500},     // 玩家活三
        {"11", -100},      // 玩家活二

        // --- 【AI防守棋形】---
        // 【最高优先级】阻挡AI下一步必胜


        // 【次高优先级】阻挡AI形成绝对优势
        {"1222", -650},   // 阻挡AI活三
        {"122", -150},     // 阻挡AI活二
        { "122222", -50000 }, // 阻挡AI五连
        {"12222", -45000},  // 阻挡AI冲四
    };

    int i = bestMove.size();
    if (step == 3) {
        bestMove[i - 1] = { "122222",-65000 };
        bestMove[i - 2] = { "12222",-60000 };
    }

    // 棋盘中心权重（初期布局优先中心）
    int center = BOARDSIZE / 2;
    int scoreBoard[BOARDSIZE][BOARDSIZE] = { 0 };
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            scoreBoard[i][j] = BOARDSIZE - (abs(center - i) + abs(center - j));
        }
    }

    multimap<int, pair<int, int>> topTenScore; // 存储Top10高分落子点（负分，越小越好）

    // 遍历所有空位，评估落子价值
    for (int y = 0; y < BOARDSIZE; y++) {
        for (int x = 0; x < BOARDSIZE; x++) {
            if (isValid(x, y)) {
                // 模拟玩家落子
                tempboard[y][x] = 1;
                vector<string> lines = getChessLines(x, y, tempboard);
                int totalScore = 0;
                bool foundPattern = false;

                // 计算当前落子位置的分数
                for (auto line : lines) {
                    for (auto pattern : bestMove) {
                        if (line.find(pattern.first) != string::npos) {
                            totalScore += pattern.second;
                            foundPattern = true;
                        }
                    }
                }
                if (!foundPattern) {
                    totalScore = -scoreBoard[y][x]; // 注意：这里是减，因为分数是负值
                }

                // 将当前落子位置和分数加入分数容器
                if (topTenScore.size() < TOP_N) {
                    topTenScore.insert({ totalScore, {x, y} });
                }
                else if (totalScore < topTenScore.rbegin()->first) { // 注意：负分，越小越好
                    topTenScore.erase(--topTenScore.end());
                    topTenScore.insert({ totalScore, {x, y} });
                }

                // 撤销模拟落子
                tempboard[y][x] = 0;
            }
        }
    }

    return topTenScore;
}

