// 旧版 AI 快照：仅用于测试/基准里和旧算法做自我对弈对比，不参与游戏运行。
#pragma once

#include <utility>
#include <vector>

#include "core/GameTypes.h"

// 计算旧版 AI 本回合的落子。side：1 黑 / 2 白（内部做颜色翻转）。
std::vector<std::pair<int, int>> aiput_legacy(int tempboard[BOARDSIZE][BOARDSIZE], int side = 2);
