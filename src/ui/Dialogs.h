// 对局中的对话框与结束按钮
#pragma once

// 退出确认框的三种选择
enum class ExitChoice { Resume, BackToMenu, QuitGame };

// 对局结束后的选择
enum class EndChoice { Restart, BackToMenu, QuitGame };

// 半透明压暗整个窗口，让确认框成为视觉焦点
void DrawDimOverlay();
// 对局中弹出模态确认框：继续游戏 / 返回主菜单 / 退出游戏
ExitChoice ShowExitConfirm();
// 对局结束后的按钮：重新开始 / 返回菜单 / 退出游戏
EndChoice GameEndButtons();
