// 局域网对战 —— 共享界面工具（尺寸、配色、图片资源、绘制与按钮）
#pragma once

#include <graphics.h>

#include <string>

#include "ui/Button.h"

namespace obgame { class Game; }

constexpr int kWindowW = 808;
constexpr int kWindowH = 608;
constexpr int kBoardPixels = 608;
constexpr int kCellSize = 32;

const COLORREF kTextBlack = RGB(0, 0, 0);
const COLORREF kTextRed = RGB(255, 0, 0);
const COLORREF kButtonBg = RGB(208, 208, 208);
const COLORREF kWhite = RGB(255, 255, 255);
const COLORREF kFocusBorder = RGB(52, 120, 210); // 输入框获得焦点时的边框色
const COLORREF kTextHint = RGB(96, 96, 96);      // 输入框内的灰色提示文字

const wchar_t* const kFaceLi = L"隶书";
const wchar_t* const kFaceYahei = L"微软雅黑";

// 与原版 main.cpp 的 LoadImages/DrawBoard 使用同一套图片
extern IMAGE g_menuBg;
extern IMAGE g_boardBack;
extern IMAGE g_blackStone;
extern IMAGE g_whiteStone;
extern IMAGE g_buffer;

// UTF-8 转宽字符（用于把网络错误信息显示到界面）
std::wstring Widen(const std::string& text);

// 宽字符转 UTF-8（用于发送聊天消息）
std::string Narrow(const std::wstring& text);

// 把网络层的英文错误串翻译成界面可读的中文提示
std::wstring DescribeNetError(const std::string& error);

void DrawTextAt(int x, int y, int size, COLORREF color, const wchar_t* text, const wchar_t* face = kFaceYahei);
void DrawTextCentered(int centerX, int y, int size, COLORREF color, const wchar_t* text, const wchar_t* face = kFaceYahei);

// 与原版一致的按钮工厂：灰白底、黑字、隶书
Button MakeButton(int x, int y, int w, int h, const wchar_t* text, int fontSize);
bool HitButton(const Button& button, int x, int y);

bool LoadLanImages();
void DrawMenuBackground();
void DrawBoardToBuffer(const obgame::Game& game);

// 简单的模态提示框（白底 + 标题 + 可选说明 + 返回按钮）
void ShowMessage(const std::wstring& title, const std::wstring& detail = std::wstring());
