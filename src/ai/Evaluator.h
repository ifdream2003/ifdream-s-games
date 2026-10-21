// AI 落子评估（局面打分）
#pragma once

#include <map>
#include <utility>

#include "core/GameTypes.h"

const int TOP_N = 5; // 计算分支数量

// AI 落子评估函数
std::multimap<int, std::pair<int, int>> aiMove(int tempboard[][BOARDSIZE], int step);
// 玩家落子评估函数
std::multimap<int, std::pair<int, int>> playerMove(int tempboard[][BOARDSIZE], int step);
