// 棋盘绘制与棋盘侧按钮
#pragma once

#include "ui/Button.h"

// 仅人机对战页显示并响应退出按钮
extern bool gShowExitButton;
extern Button undoButton;
extern Button exitButton;

// 打印棋盘
void DrawBoard();

// 抹掉右侧「退出」按钮（对局结束后隐藏，不改变 gShowExitButton，下一局会重新绘制）
void HideExitButton();
// 抹掉右侧「悔棋」按钮（对局结束时与退出按钮一起隐藏）
void HideUndoButton();
