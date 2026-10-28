// 连六棋 AI：一回合两子组合的 Alpha-Beta 搜索 + 威胁优先排序。
//
// 对外接口保持兼容：aiput() 默认按白棋（2）计算一个回合的两颗子。
// 测试/基准程序可以传入 side 让 AI 执黑，也可以把单步搜索时间调小。
#pragma once

#include <utility>
#include <vector>

#include "core/GameTypes.h"

namespace ai {

// 单步思考时间上限（毫秒），默认 500
void SetTimeLimitMs(int ms);
int GetTimeLimitMs();

// 最大搜索深度（以“子”为单位，建议偶数），默认 20
void SetMaxDepth(int depth);

// 最近一次 aiput 的统计信息
struct SearchStats {
    long long nodes = 0;
    int depth = 0;
    long long elapsedMs = 0;
    bool timedOut = false;
};
SearchStats GetLastStats();

} // namespace ai

// 计算 AI 本回合的落子。side：1 黑 / 2 白。
// 返回至少 1 个合法落子；正常回合返回 2 个（不重复、不越界）。
std::vector<std::pair<int, int>> aiput(int tempboard[BOARDSIZE][BOARDSIZE], int side = 2);
