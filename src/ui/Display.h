// 显示层：808×608 逻辑画布 + 等比缩放到窗口（窗口模式可自由缩放，多余区域留黑边），并做鼠标坐标反算
#pragma once

#include <graphics.h>
#include <windows.h>

namespace ui {

constexpr int kLogicalW = 808;
constexpr int kLogicalH = 608;

// 首次创建窗口（默认窗口模式，可拖拽边框放大缩小）并把绘图目标切到离屏画布；已初始化时只切回画布
void InitDisplay();
void EnsureDisplay();
void ShutdownDisplay();

// 全屏 / 窗口切换（F11 也会触发）
void SetFullscreen(bool enabled);
bool IsFullscreen();
void ToggleFullscreen();

// 窗口模式下的等比缩放：Ctrl + / Ctrl - 触发，Ctrl 0 复位到默认窗口大小
void ZoomWindow(double factor);
void ResetWindowSize();

// 把画布等比缩放到窗口中间并补黑边；每次绘制完成后调用
void Present();

// 空闲循环里调用：窗口尺寸变化时自动重绘，拖动边框缩放时画面实时跟随
void Pump();

// 窗口坐标 → 808×608 逻辑坐标；返回 false 表示落在黑边上
bool MapWindowPoint(int winX, int winY, int* logicalX, int* logicalY);

// 输入封装：坐标已映射为逻辑坐标，未命中画布时置为 -1
MOUSEMSG GetMouseMsgMapped();
bool PeekMessageMapped(ExMessage* msg, BYTE filter = static_cast<BYTE>(-1));
bool GetMessageMapped(ExMessage* msg, BYTE filter = static_cast<BYTE>(-1));

} // namespace ui
