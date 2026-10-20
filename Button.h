#pragma once
#include <graphics.h>
#include <string>

using namespace std;

class Button {
private:
    int x, y, w, h;
    wstring text;
    wstring font;
    int fontSize;
    COLORREF backgroundColor; 
    COLORREF textColor;       
    bool isHovered;

public:
    Button(int x, int y, int w, int h, const wstring& text,
        COLORREF backgroundColor = RGB(255, 255, 255),  // 默认白色
        COLORREF textColor = RGB(0, 0, 0))              // 默认黑色
        : x(x), y(y), w(w), h(h), text(text),
        backgroundColor(backgroundColor), textColor(textColor), isHovered(false) {
    }

    void setFontSize(int fontSize) {
        this->fontSize = fontSize;
    }

    void setFont(wstring font) {
        this->font = font;
    }

    void setBackgroundColor(COLORREF backgroundColor) {
        this->backgroundColor = backgroundColor;
    }

    void setTextColor(COLORREF textColor) {
        this->textColor = textColor;
    }

    // 判断鼠标是否在按钮上
    bool IsMouseOnButton(const MOUSEMSG& m) const {
        return m.x >= x && m.x <= x + w &&
            m.y >= y && m.y <= y + h;
    }

    void DrawButton() {
        // 绘制按钮背景
        setfillcolor(backgroundColor);
        fillrectangle(x, y, x + w, y + h);

        // 绘制按钮文字
        setbkmode(TRANSPARENT);
        settextcolor(textColor);
        settextstyle(fontSize, 0, font.c_str());
        outtextxy(
            x + (w - textwidth(text.c_str())) / 2,
            y + (h - textheight(text.c_str())) / 2,
            text.c_str()
        );
    }
};