#include "ui/Assets.h"

#include <windows.h>

IMAGE white, black, menu, back, buffer(WINDOW_WIDTH, WINDOW_HEIGHT);

// 返回 exe 同目录下 assets/<fileName> 的绝对路径。
// 构建后会把 assets/ 复制到输出目录，因此程序可以从任意工作目录启动。
std::wstring AssetPath(const wchar_t* fileName) {
    wchar_t exePath[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath, length > 0 ? static_cast<size_t>(length) : 0);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        dir.resize(slash + 1);
    }
    else {
        dir.clear();
    }
    return dir + L"assets\\" + fileName;
}

// 加载图片（资源随构建复制到 exe 同目录的 assets/ 下）
void LoadImages() {
    loadimage(&white, AssetPath(L"white.png").c_str());
    loadimage(&black, AssetPath(L"black.png").c_str());
    loadimage(&menu, AssetPath(L"menu.png").c_str());
    loadimage(&back, AssetPath(L"board.png").c_str());
}
