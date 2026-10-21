// 图片资源与加载
#pragma once

#include <graphics.h>

#include <string>

#include "ui/Theme.h"

extern IMAGE white, black, menu, back, buffer;

// 返回 exe 同目录下 assets/<fileName> 的绝对路径
std::wstring AssetPath(const wchar_t* fileName);

// 加载图片
void LoadImages();
