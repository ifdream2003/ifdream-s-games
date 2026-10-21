// 胜负判断与棋形提取
#pragma once

#include <string>
#include <vector>

#include "core/GameState.h"

// 获取某个位置的四条线
std::vector<std::string> getLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
// 判断胜负
int checkWin(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
// 复制棋盘
void copyBoard(int source[BOARDSIZE][BOARDSIZE], int dest[BOARDSIZE][BOARDSIZE]);
// 获取棋形线条（AI 评估用）
std::vector<std::string> getChessLines(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
