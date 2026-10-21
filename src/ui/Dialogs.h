// 对局中的对话框与结束按钮
#pragma once

// 对局中退出确认框的两种选择
enum class ExitChoice { Resume, BackToMenu };

// 对局结束后的选择
enum class EndChoice { Restart, BackToMenu };

// 半透明压暗整个窗口，让确认框成为视觉焦点
void DrawDimOverlay();
// 对局中弹出模态确认框：继续游戏 / 返回菜单（联机模式传「返回大厅」），Esc 同样继续对局
ExitChoice ShowExitConfirm(const wchar_t* menuText = L"返回菜单");
// 对局结果遮罩：白底黑框 + 居中标题（棋盘中央显示胜负）
void DrawResultOverlay(const wchar_t* title);
// 对局结束后的按钮：重新开始 / 返回菜单（不再提供直接退出游戏的入口）
EndChoice GameEndButtons();
