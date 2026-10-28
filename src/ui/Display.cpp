#include "ui/Display.h"

#include <algorithm>
#include <cstring>

#pragma comment(lib, "MSIMG32.LIB")

namespace ui {

namespace {

IMAGE g_canvas(kCanvasW, kCanvasH);

// 画布按 kSupersample 倍超采样绘制；切到窗口/帧缓冲做 1:1 拷贝时要临时恢复 1 倍
void ApplyCanvasAspect() {
    setaspectratio(static_cast<float>(kSupersample), static_cast<float>(kSupersample));
}
void ApplyPixelAspect() {
    setaspectratio(1.0f, 1.0f);
}

// 1:1 位图拷贝（GDI 的 StretchBlt / BitBlt / AlphaBlend）的前后处理。
// EasyX 的超采样是靠 setaspectratio 作用在“当前绘图设备”的 DC 上实现的，而 GDI 的位图
// 拷贝在源、目标 DC 的纵横比不一致时行为不可靠：源画布仍带 3 倍纵横比时，StretchBlt 会把
// 目标矩形按 1/kSupersample 缩小，整屏画面缩到窗口左上角、其余区域全黑。
// 所以拷贝前把画布 DC 恢复 1:1，拷贝后再切回 3 倍。
IMAGE* BeginCanvasBlit() {
    IMAGE* previous = GetWorkingImage();
    SetWorkingImage(&g_canvas);
    ApplyPixelAspect();
    return previous;
}
void EndCanvasBlit(IMAGE* previous) {
    SetWorkingImage(&g_canvas);
    ApplyCanvasAspect();
    SetWorkingImage(previous);
}

bool g_initialized = false;
bool g_fullscreen = false; // 默认窗口模式：拖拽边框即可放大缩小
HWND g_hwnd = nullptr;

bool g_f11Down = false;
bool g_zoomInDown = false;
bool g_zoomOutDown = false;
bool g_zoomResetDown = false;

IMAGE* g_frame = nullptr;
int g_frameW = 0;
int g_frameH = 0;

// EasyX 窗口画布当前尺寸。用户拖动边框后客户区变了、EasyX 画布还是旧尺寸，
// 必须用 Resize(nullptr, ...) 同步，否则放大后的区域会被裁掉
int g_surfaceW = 0;
int g_surfaceH = 0;

// 上一帧呈现时的客户区尺寸：Pump 据此判断是否需要重绘
int g_presentW = 0;
int g_presentH = 0;

// 窗口模式下记住的客户区尺寸：全屏与窗口来回切换时用它恢复
int g_windowW = 0;
int g_windowH = 0;

constexpr double kMinScale = 0.45;
constexpr double kMaxScale = 3.0;
constexpr double kZoomStep = 1.1;

struct Layout {
    int winW = kLogicalW;
    int winH = kLogicalH;
    int x = 0;
    int y = 0;
    int w = kLogicalW;
    int h = kLogicalH;
    double scale = 1.0;
};

int ScreenW() {
    return GetSystemMetrics(SM_CXSCREEN);
}

int ScreenH() {
    return GetSystemMetrics(SM_CYSCREEN);
}

// windows.h 里的 min/max 宏会和 std::min/std::max 冲突，这里用两个小工具函数
template <typename T>
T MinOf(T a, T b) {
    return a < b ? a : b;
}

template <typename T>
T MaxOf(T a, T b) {
    return a > b ? a : b;
}

// 默认窗口大小：1.4 倍逻辑画布，屏幕较小时自动收小，保证标题栏和边框放得下
void DefaultClientSize(int* w, int* h) {
    double scale = 1.4;
    const double byW = (ScreenW() - 80.0) / kLogicalW;
    const double byH = (ScreenH() - 120.0) / kLogicalH;
    scale = MinOf(scale, MinOf(byW, byH));
    if (scale < 0.75) {
        scale = 0.75;
    }
    *w = static_cast<int>(kLogicalW * scale + 0.5);
    *h = static_cast<int>(kLogicalH * scale + 0.5);
}

bool KeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// 让 EasyX 的窗口画布跟随当前客户区尺寸（仅窗口模式使用）
void SyncWindowSurface() {
    if (g_hwnd == nullptr) {
        return;
    }
    if (g_fullscreen) {
        // 全屏是无边框窗口，画布尺寸由 ApplyWindowMode 直接设定。
        // 这里若调用 Resize(nullptr,...)，EasyX 会按带标题栏的样式换算客户区，
        // 客户区每帧被撑大一点，画面就会朝左上角无限放大。
        return;
    }
    RECT rc;
    if (!GetClientRect(g_hwnd, &rc)) {
        return;
    }
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) {
        return;
    }
    if (cw == g_surfaceW && ch == g_surfaceH) {
        return;
    }
    Resize(nullptr, cw, ch);
    g_surfaceW = cw;
    g_surfaceH = ch;
}

void ApplyWindowMode() {
    if (g_hwnd == nullptr) {
        return;
    }
    if (g_fullscreen) {
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hwnd, HWND_TOP, 0, 0, ScreenW(), ScreenH(),
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        // 全屏窗口是无边框的：initgraph 已经按屏幕尺寸建好画布，
        // 这里直接把画布尺寸记成屏幕尺寸，绝不能调用 Resize(nullptr,...)。
        // EasyX 的 Resize 会按创建窗口时的带标题栏样式换算客户区，
        // 与无边框全屏不一致，客户区会逐帧变大，画面朝左上角无限放大。
        g_surfaceW = ScreenW();
        g_surfaceH = ScreenH();
    } else {
        if (g_windowW <= 0 || g_windowH <= 0) {
            DefaultClientSize(&g_windowW, &g_windowH);
        }
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        RECT rc = {0, 0, g_windowW, g_windowH};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        const int ww = rc.right - rc.left;
        const int wh = rc.bottom - rc.top;
        int x = (ScreenW() - ww) / 2;
        int y = (ScreenH() - wh) / 2;
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        SetWindowPos(g_hwnd, HWND_TOP, x, y, ww, wh,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_surfaceW = g_windowW;
        g_surfaceH = g_windowH;
    }
}

Layout ComputeLayout() {
    Layout layout;
    if (g_hwnd != nullptr) {
        RECT rc;
        if (GetClientRect(g_hwnd, &rc)) {
            layout.winW = rc.right - rc.left;
            layout.winH = rc.bottom - rc.top;
        }
    }
    if (layout.winW <= 0 || layout.winH <= 0) {
        layout.winW = kLogicalW;
        layout.winH = kLogicalH;
    }
    const double scaleX = static_cast<double>(layout.winW) / kLogicalW;
    const double scaleY = static_cast<double>(layout.winH) / kLogicalH;
    layout.scale = scaleX < scaleY ? scaleX : scaleY;
    layout.w = static_cast<int>(kLogicalW * layout.scale);
    layout.h = static_cast<int>(kLogicalH * layout.scale);
    layout.x = (layout.winW - layout.w) / 2;
    layout.y = (layout.winH - layout.h) / 2;
    return layout;
}

// 按客户区尺寸调整窗口，窗口中心保持不动
void ResizeWindowClient(int clientW, int clientH) {
    if (g_hwnd == nullptr) {
        return;
    }
    if (clientW < 240) {
        clientW = 240;
    }
    if (clientH < 180) {
        clientH = 180;
    }
    RECT wr;
    GetWindowRect(g_hwnd, &wr);
    RECT rc = {0, 0, clientW, clientH};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtr(g_hwnd, GWL_STYLE));
    AdjustWindowRect(&rc, style, FALSE);
    const int ww = rc.right - rc.left;
    const int wh = rc.bottom - rc.top;
    const int cx = (wr.left + wr.right) / 2;
    const int cy = (wr.top + wr.bottom) / 2;
    SetWindowPos(g_hwnd, HWND_TOP, cx - ww / 2, cy - wh / 2, ww, wh,
                 SWP_NOZORDER | SWP_SHOWWINDOW);
    g_windowW = clientW;
    g_windowH = clientH;
    g_surfaceW = 0;
    g_surfaceH = 0;
    SyncWindowSurface();
}

void HandleHotkeys() {
    const bool ctrl = KeyDown(VK_CONTROL);

    const bool f11 = KeyDown(VK_F11);
    if (f11 && !g_f11Down) {
        g_f11Down = true;
        ToggleFullscreen();
    } else if (!f11) {
        g_f11Down = false;
    }

    const bool zoomIn = ctrl && (KeyDown(VK_OEM_PLUS) || KeyDown(VK_ADD));
    if (zoomIn && !g_zoomInDown) {
        g_zoomInDown = true;
        ZoomWindow(kZoomStep);
    } else if (!zoomIn) {
        g_zoomInDown = false;
    }

    const bool zoomOut = ctrl && (KeyDown(VK_OEM_MINUS) || KeyDown(VK_SUBTRACT));
    if (zoomOut && !g_zoomOutDown) {
        g_zoomOutDown = true;
        ZoomWindow(1.0 / kZoomStep);
    } else if (!zoomOut) {
        g_zoomOutDown = false;
    }

    const bool zoomReset = ctrl && (KeyDown('0') || KeyDown(VK_NUMPAD0));
    if (zoomReset && !g_zoomResetDown) {
        g_zoomResetDown = true;
        ResetWindowSize();
    } else if (!zoomReset) {
        g_zoomResetDown = false;
    }
}

} // namespace

void EnsureDisplay() {
    if (!g_initialized) {
        InitDisplay();
        return;
    }
    SetWorkingImage(&g_canvas);
    ApplyCanvasAspect();
}

void InitDisplay() {
    if (g_initialized) {
        EnsureDisplay();
        return;
    }
    if (g_windowW <= 0 || g_windowH <= 0) {
        DefaultClientSize(&g_windowW, &g_windowH);
    }
    g_hwnd = initgraph(g_fullscreen ? ScreenW() : g_windowW,
                       g_fullscreen ? ScreenH() : g_windowH);
    ApplyWindowMode();
    SetWorkingImage(&g_canvas);
    ApplyCanvasAspect();
    g_initialized = true;
}

void ShutdownDisplay() {
    delete g_frame;
    g_frame = nullptr;
    g_frameW = 0;
    g_frameH = 0;
    closegraph();
    g_hwnd = nullptr;
    g_initialized = false;
}

void SetFullscreen(bool enabled) {
    if (!g_initialized || g_fullscreen == enabled) {
        return;
    }
    if (enabled) {
        // 记住当前窗口尺寸，退出全屏时恢复
        RECT rc;
        if (g_hwnd != nullptr && GetClientRect(g_hwnd, &rc)) {
            const int cw = rc.right - rc.left;
            const int ch = rc.bottom - rc.top;
            if (cw > 0 && ch > 0) {
                g_windowW = cw;
                g_windowH = ch;
            }
        }
    }
    g_fullscreen = enabled;

    // 全屏窗是无边框的，而 EasyX 的 Resize(nullptr,...) 一直按创建窗口时的带边框样式换算客户区，
    // 两者混用会让客户区逐帧变大、画面无限放大。所以切换全屏/窗口时重建窗口：
    // 画布尺寸直接由 initgraph 决定，和客户区严格一致。
    wchar_t title[128] = {0};
    if (g_hwnd != nullptr) {
        GetWindowTextW(g_hwnd, title, 128);
    }
    delete g_frame;
    g_frame = nullptr;
    g_frameW = 0;
    g_frameH = 0;
    g_surfaceW = 0;
    g_surfaceH = 0;
    g_presentW = 0;
    g_presentH = 0;

    closegraph();
    g_hwnd = initgraph(g_fullscreen ? ScreenW() : g_windowW,
                       g_fullscreen ? ScreenH() : g_windowH);
    if (title[0] != 0 && g_hwnd != nullptr) {
        SetWindowTextW(g_hwnd, title);
    }
    ApplyWindowMode();
    if (g_hwnd != nullptr) {
        SetForegroundWindow(g_hwnd); // 重建窗口后把焦点拿回来，Esc / 回车等按键继续有效
    }
    SetWorkingImage(&g_canvas);
    ApplyCanvasAspect();
    Present();
}

bool IsFullscreen() {
    return g_fullscreen;
}

void ToggleFullscreen() {
    SetFullscreen(!g_fullscreen);
}

void ZoomWindow(double factor) {
    if (!g_initialized || g_fullscreen || factor <= 0.0) {
        return;
    }
    RECT rc;
    if (!GetClientRect(g_hwnd, &rc)) {
        return;
    }
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    double current = MinOf(static_cast<double>(cw) / kLogicalW,
                           static_cast<double>(ch) / kLogicalH);
    if (current <= 0.0) {
        current = 1.0;
    }
    double next = current * factor;
    // 上限同时受屏幕尺寸约束，避免窗口比显示器还大
    const double byW = (ScreenW() - 60.0) / kLogicalW;
    const double byH = (ScreenH() - 100.0) / kLogicalH;
    const double maxScale = MinOf(kMaxScale, MinOf(byW, byH));
    next = MinOf(next, maxScale);
    next = MaxOf(next, kMinScale);
    ResizeWindowClient(static_cast<int>(kLogicalW * next + 0.5),
                       static_cast<int>(kLogicalH * next + 0.5));
}

void ResetWindowSize() {
    if (!g_initialized || g_fullscreen) {
        return;
    }
    int w = 0;
    int h = 0;
    DefaultClientSize(&w, &h);
    ResizeWindowClient(w, h);
}

// 窗口大小的帧缓冲：先在离屏帧上铺黑边并拉伸画布，再整帧贴到窗口，避免闪烁
void EnsureFrame(int winW, int winH) {
    if (g_frame != nullptr && g_frameW == winW && g_frameH == winH) {
        return;
    }
    delete g_frame;
    g_frame = new IMAGE(winW, winH);
    g_frameW = winW;
    g_frameH = winH;
}

void Present() {
    if (!g_initialized) {
        return;
    }
    HandleHotkeys();
    SyncWindowSurface(); // 用户拖动边框改过窗口大小后，先把 EasyX 画布同步过来
    const Layout layout = ComputeLayout();
    if (layout.winW <= 0 || layout.winH <= 0) {
        return;
    }
    EnsureFrame(layout.winW, layout.winH);
    if (g_frame == nullptr) {
        return;
    }
    // 源画布 DC 必须先恢复 1:1，否则 StretchBlt 会把画面缩成左上角一小块（见 BeginCanvasBlit）
    IMAGE* previous = BeginCanvasBlit();
    HDC dstDc = GetImageHDC(g_frame);
    HDC srcDc = GetImageHDC(&g_canvas);

    SetWorkingImage(g_frame);
    ApplyPixelAspect();
    setfillcolor(RGB(0, 0, 0));
    solidrectangle(0, 0, layout.winW, layout.winH);
    if (layout.w > 0 && layout.h > 0) {
        // 先把超采样画布按 HALFTONE 缩放到窗口帧缓冲（1:1 时不会有二次缩放）
        SetStretchBltMode(dstDc, HALFTONE);
        SetBrushOrgEx(dstDc, 0, 0, nullptr);
        StretchBlt(dstDc, layout.x, layout.y, layout.w, layout.h,
                   srcDc, 0, 0, kCanvasW, kCanvasH, SRCCOPY);
    }
    SetWorkingImage(nullptr);
    ApplyPixelAspect();
    BeginBatchDraw();
    putimage(0, 0, g_frame);
    EndBatchDraw();
    EndCanvasBlit(previous);
    g_presentW = layout.winW;
    g_presentH = layout.winH;
}

// 空闲循环里调用：只有窗口尺寸变了才重绘，保证拖动边框时缩放实时跟随而几乎不耗 CPU
void Pump() {
    if (!g_initialized || g_hwnd == nullptr) {
        return;
    }
    HandleHotkeys();
    RECT rc;
    if (!GetClientRect(g_hwnd, &rc)) {
        return;
    }
    const int cw = rc.right - rc.left;
    const int ch = rc.bottom - rc.top;
    if (cw != g_presentW || ch != g_presentH) {
        Present();
    }
}

// ---------------------------------------------------------------------------
// 超采样画布上的图片绘制 / 快照工具
// 说明：这些工具都在画布上做 1:1 位图拷贝，必须先用 BeginCanvasBlit() 把画布 DC
// 恢复成 1:1，拷贝完再用 EndCanvasBlit() 切回 kSupersample。
// ---------------------------------------------------------------------------

void DrawCanvasImage(const IMAGE* image) {
    if (image == nullptr || image->getwidth() <= 0 || image->getheight() <= 0) {
        return;
    }
    IMAGE* previous = BeginCanvasBlit();
    HDC dst = GetImageHDC(&g_canvas);
    HDC src = GetImageHDC(const_cast<IMAGE*>(image));
    SetStretchBltMode(dst, HALFTONE);
    SetBrushOrgEx(dst, 0, 0, nullptr);
    StretchBlt(dst, 0, 0, kCanvasW, kCanvasH,
               src, 0, 0, image->getwidth(), image->getheight(), SRCCOPY);
    EndCanvasBlit(previous);
}

void DrawCanvasImageAlpha(const IMAGE* image, int logicalX, int logicalY) {
    if (image == nullptr) {
        return;
    }
    const int w = image->getwidth();
    const int h = image->getheight();
    if (w <= 0 || h <= 0) {
        return;
    }
    IMAGE* previous = BeginCanvasBlit();
    HDC dst = GetImageHDC(&g_canvas);
    HDC src = GetImageHDC(const_cast<IMAGE*>(image));
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(dst, logicalX * kSupersample, logicalY * kSupersample, w, h,
               src, 0, 0, w, h, blend);
    EndCanvasBlit(previous);
}

void SnapshotCanvas(IMAGE* snapshot) {
    if (snapshot == nullptr) {
        return;
    }
    IMAGE* previous = BeginCanvasBlit();
    HDC dst = GetImageHDC(snapshot);
    HDC src = GetImageHDC(&g_canvas);
    BitBlt(dst, 0, 0, kCanvasW, kCanvasH, src, 0, 0, SRCCOPY);
    EndCanvasBlit(previous);
}

void RestoreCanvas(const IMAGE* snapshot) {
    if (snapshot == nullptr) {
        return;
    }
    IMAGE* previous = BeginCanvasBlit();
    HDC dst = GetImageHDC(&g_canvas);
    HDC src = GetImageHDC(const_cast<IMAGE*>(snapshot));
    BitBlt(dst, 0, 0, kCanvasW, kCanvasH, src, 0, 0, SRCCOPY);
    EndCanvasBlit(previous);
}

void DimCanvas(int alpha) {
    IMAGE dim(kCanvasW, kCanvasH);
    IMAGE* previous = BeginCanvasBlit();

    // 遮罩图本身也按 1:1 涂黑（新 IMAGE 的纵横比默认就是 1 倍）
    SetWorkingImage(&dim);
    ApplyPixelAspect();
    setfillcolor(RGB(0, 0, 0));
    solidrectangle(0, 0, kCanvasW, kCanvasH);

    SetWorkingImage(&g_canvas);
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
    blend.AlphaFormat = 0;
    AlphaBlend(GetImageHDC(&g_canvas), 0, 0, kCanvasW, kCanvasH,
               GetImageHDC(&dim), 0, 0, kCanvasW, kCanvasH, blend);

    EndCanvasBlit(previous);
}

bool MapWindowPoint(int winX, int winY, int* logicalX, int* logicalY) {
    const Layout layout = ComputeLayout();
    if (layout.scale <= 0.0) {
        return false;
    }
    const int x = static_cast<int>((winX - layout.x) / layout.scale);
    const int y = static_cast<int>((winY - layout.y) / layout.scale);
    if (x < 0 || x >= kLogicalW || y < 0 || y >= kLogicalH) {
        return false;
    }
    if (logicalX != nullptr) {
        *logicalX = x;
    }
    if (logicalY != nullptr) {
        *logicalY = y;
    }
    return true;
}

namespace {

bool IsMouseMessage(UINT uMsg) {
    switch (uMsg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
        return true;
    default:
        return false;
    }
}

MOUSEMSG MakeMouseMsg(const ExMessage& em) {
    MOUSEMSG msg;
    memset(&msg, 0, sizeof(msg));
    msg.uMsg = em.message;
    msg.mkCtrl = em.ctrl;
    msg.mkShift = em.shift;
    msg.mkLButton = em.lbutton;
    msg.mkMButton = em.mbutton;
    msg.mkRButton = em.rbutton;
    msg.x = em.x;
    msg.y = em.y;
    msg.wheel = em.message == WM_MOUSEWHEEL ? em.wheel : 0;
    return msg;
}

} // namespace

// 取一条鼠标消息。没有消息时不干等：持续 Pump 窗口，让拖动边框调整大小时布局实时缩放
MOUSEMSG GetMouseMsgMapped() {
    Present();
    ExMessage em;
    while (true) {
        while (peekmessage(&em, EX_MOUSE)) {
            if (!IsMouseMessage(em.message)) {
                continue;
            }
            MOUSEMSG msg = MakeMouseMsg(em);
            int x = -1;
            int y = -1;
            MapWindowPoint(em.x, em.y, &x, &y);
            msg.x = static_cast<short>(x);
            msg.y = static_cast<short>(y);
            return msg;
        }
        Pump();
        Sleep(1);
    }
}

bool PeekMessageMapped(ExMessage* msg, BYTE filter) {
    if (!peekmessage(msg, filter)) {
        return false;
    }
    if (msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONDOWN ||
        msg->message == WM_LBUTTONUP || msg->message == WM_LBUTTONDBLCLK ||
        msg->message == WM_RBUTTONDOWN || msg->message == WM_RBUTTONUP ||
        msg->message == WM_MBUTTONDOWN || msg->message == WM_MBUTTONUP) {
        int x = -1;
        int y = -1;
        MapWindowPoint(msg->x, msg->y, &x, &y);
        msg->x = x;
        msg->y = y;
    }
    return true;
}

bool GetMessageMapped(ExMessage* msg, BYTE filter) {
    Present();
    while (!peekmessage(msg, filter)) {
        // 同样不阻塞：等待消息时保持窗口尺寸同步
        Pump();
        Sleep(1);
    }
    if (msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONDOWN ||
        msg->message == WM_LBUTTONUP || msg->message == WM_LBUTTONDBLCLK ||
        msg->message == WM_RBUTTONDOWN || msg->message == WM_RBUTTONUP ||
        msg->message == WM_MBUTTONDOWN || msg->message == WM_MBUTTONUP) {
        int x = -1;
        int y = -1;
        MapWindowPoint(msg->x, msg->y, &x, &y);
        msg->x = x;
        msg->y = y;
    }
    return true;
}

} // namespace ui
