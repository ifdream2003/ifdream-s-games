#include "core/GameState.h"

#include <iostream>

using namespace std;

int board[BOARDSIZE][BOARDSIZE] = { 0 }; // 0空，1黑，2白
int currentPlayer = 1;   // 1黑，2白

// 落子记录
stack<pair<int, int>> moveHistory = {};

//记录一步棋
void recordMove(pair<int, int> location) {
    std::cout << "Recording move at: (" << location.first << ", " << location.second << ")\n";
    moveHistory.push(location);
}

// 清空所有历史记录
void clearHistory() {
    std::cout << "Clearing move history.\n";
    moveHistory = {};
}

// 检查是否可以悔棋
bool canUndo() {
    return !moveHistory.empty();
}

// 悔棋功能
void undoMove(int state) {
    // 首先检查是否有历史记录可以撤销
    if (!canUndo()) {
        std::cout << "Cannot undo. No moves have been made.\n";
        return; // 什么都不做，直接返回
    }
    pair<int, int> locate;
    if (state == 1) {
        if (moveHistory.size() < 4) {//小于4，说明玩家回退的是第一步
            for (int i = 0; i < 3; i++) {
                locate = moveHistory.top();
                board[locate.second][locate.first] = 0;
                moveHistory.pop();
            }
        }
        else {//大于4的处理方式
            for (int i = 0; i < 4; i++) {
                locate = moveHistory.top();
                board[locate.second][locate.first] = 0;
                moveHistory.pop();
            }
        }
    }
    if (state == 2) {
        if (moveHistory.size() < 2) {//小于2，说明玩家回退的是第一步
            locate = moveHistory.top();
            board[locate.second][locate.first] = 0;
            moveHistory.pop();
        }
        else {//大于2的处理方式
            for (int i = 0; i < 2; i++) {
                locate = moveHistory.top();
                board[locate.second][locate.first] = 0;
                moveHistory.pop();
            }
        }
    }
}

// 判断落子是否合法函数
bool isValid(int x, int y, int tempboard[BOARDSIZE][BOARDSIZE]) {
    if (x < 0 || x >= BOARDSIZE || y < 0 || y >= BOARDSIZE) {
        return false;
    }
    if (tempboard[y][x] != 0) {
        return false;
    }
    return true;
}

// 放置棋子函数
bool setchess(int x, int y, int chess, int tempboard[BOARDSIZE][BOARDSIZE]) {
    if (!isValid(x, y, tempboard)) {
        cout << "Invalid move! (" << x << "," << y << ")" << endl;
        return false;
    }
    tempboard[y][x] = chess;
    return true;
}

