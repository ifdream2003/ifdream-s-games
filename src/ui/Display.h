// 显示层：808×608 逻辑画布 + 等比缩放到窗口（窗口模式可自由缩放，多余区域留黑边），并做鼠标坐标反算
#pragma once

#include <graphics.h>
#include <windows.h>

namespace ui {

constexpr int kLogicalW = 808;
constexpr int kLogicalH = 608;

// 超采样倍数：所有界面仍按 808×608 逻辑坐标绘制，实际画布放大 kSupersample 倍，
// 最后再等比缩放到窗口，这样窗口放大/全屏时文字和图片不会被拉糊。
constexpr int kSupersample = 3;
constexpr int kCanvasW = kLogicalW * kSupersample;
constexpr int kCanvasH = kLogicalH * kSupersample;

// 以“逻辑坐标”为单位画整屏背景图（图片需是画布分辨率，1:1 贴到画布）
void DrawCanvasImage(const IMAGE* image);
// 按逻辑坐标画带透明通道的图片（常用于棋子，图片尺寸应为逻辑尺寸 × kSupersample）
void DrawCanvasImageAlpha(const IMAGE* image, int logicalX, int logicalY);
// 保存 / 恢复整块画布（用于对局中弹框时暂存画面）
void SnapshotCanvas(IMAGE* snapshot);
void RestoreCanvas(const IMAGE* snapshot);
// 把整块画布压暗（0~255）
void DimCanvas(int alpha);

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
