// 棋盘绘制与棋盘侧按钮
#pragma once

#include "ui/Button.h"

// 仅人机对战页显示并响应退出按钮
extern bool gShowExitButton;
extern Button undoButton;
extern Button exitButton;

// 打印棋盘
void DrawBoard();
