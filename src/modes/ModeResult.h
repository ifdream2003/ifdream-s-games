// 对局模式结束后交给主循环的结果
#pragma once

// 对局页的离开方式，由 main 的主循环决定是回主菜单还是直接退出游戏
enum class LeaveMatch { BackToMenu, QuitGame };
