#ifndef BITMAPTEXT_H
#define BITMAPTEXT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <string>
#include <sstream>   // std::ostringstream
#include <iomanip>   // std::setprecision, std::fixed

#include <GL/gl.h>

#include "UI.h"
#include "UI/UI.h"
#include "UI/font.h"

enum class textAlign {
    left,
    right,
    center
};

void RenderBitmapText(const std::string& text, textAlign align = textAlign::left, int maxPixels = 1920);

void RenderBitmapFloat(float value, textAlign align = textAlign::left, int maxPixels = 1920, const std::string& unit = "");

#endif