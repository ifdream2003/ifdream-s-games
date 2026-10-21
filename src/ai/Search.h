// AI 递归搜索与落子
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "core/GameTypes.h"

const int dep = 3; // 递归深度

// 递归搜索最有分数计算落子点
std::multimap<int, std::pair<int, int>> scoreTopTen(
    int boardtemp[BOARDSIZE][BOARDSIZE],
    int step,
    int depth);
// AI 落子函数
std::vector<std::pair<int, int>> aiput(int tempboard[BOARDSIZE][BOARDSIZE]);
