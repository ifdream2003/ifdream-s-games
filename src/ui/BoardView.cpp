#include "ui/BoardView.h"

#include <graphics.h>

#include "core/GameState.h"
#include "ui/Assets.h"
#include "ui/Display.h"
#include "ui/Panel.h"

bool gShowExitButton = false; // 仅人机对战页显示并响应退出按钮
// 右侧控制区按钮：与局域网对战保持一致（面板宽 200px，按钮 160 宽居中，左右各留 20px）
constexpr int kPanelButtonX = 628;
constexpr int kPanelButtonW = 160;
constexpr int kPanelButtonH = 36;
constexpr int kUndoButtonY = 492;
constexpr int kExitButtonY = 540;

Button undoButton = { kPanelButtonX, kUndoButtonY, kPanelButtonW, kPanelButtonH, L"悔棋", RGB(255,255,255),RGB(0,0,0)};
// 人机对战页的退出按钮，放在悔棋下方
Button exitButton = { kPanelButtonX, kExitButtonY, kPanelButtonW, kPanelButtonH, L"退出", RGB(255,255,255),RGB(0,0,0)};

void HideExitButton() {
    setfillcolor(ui::kPanelBg);
    solidrectangle(kPanelButtonX, kExitButtonY,
                   kPanelButtonX + kPanelButtonW, kExitButtonY + kPanelButtonH);
}

void HideUndoButton() {
    setfillcolor(ui::kPanelBg);
    solidrectangle(kPanelButtonX, kUndoButtonY,
                   kPanelButtonX + kPanelButtonW, kUndoButtonY + kPanelButtonH);
}

// 打印棋盘
void DrawBoard() {
    // 背景（超采样画布上是 1:1 贴图，由 Display 负责缩放/对齐）
    ui::DrawCanvasImage(&back);

    // 绘制棋子：逻辑坐标 × 超采样倍数由 Display 处理
    for (int i = 0; i < 19; i++) {
        for (int j = 0; j < 19; j++) {
            const int x = j * 32;
            const int y = i * 32;
            if (board[i][j] == 1) {
                ui::DrawCanvasImageAlpha(&black, x, y);
            }
            else if (board[i][j] == 2) {
                ui::DrawCanvasImageAlpha(&white, x, y);
            }
        }
    }

    // 右侧状态/控制区改为深色底，保证按钮与文字醒目
    ui::DrawRightPanel();
    undoButton.setFont(L"隶书");
    undoButton.setFontSize(20);
    undoButton.DrawButton();
    if (gShowExitButton) {
        exitButton.setFont(L"隶书");
        exitButton.setFontSize(20);
        exitButton.DrawButton();
    }
}
