// 本地对局的棋盘状态、落子记录与悔棋
#pragma once

#include <stack>
#include <utility>

#include "core/GameTypes.h"

// 棋盘状态：0 空，1 黑，2 白
extern int board[BOARDSIZE][BOARDSIZE];
extern int currentPlayer;
// 落子记录
extern std::stack<std::pair<int, int>> moveHistory;

// 判断落子是否合法
bool isValid(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE] = board);
// 放置棋子
bool setchess(int x, int y, int chess, int tempboard[BOARDSIZE][BOARDSIZE] = board);
// 落子记录操作
void recordMove(std::pair<int, int> location);
void clearHistory();
bool canUndo();
void undoMove(int state);
