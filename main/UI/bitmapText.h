#ifndef BITMAPTEXT_H
#define BITMAPTEXT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <string>
#include <GL/gl.h>

#include "UI/UI.h"
#include "UI/font.h"

void RenderBitmapText(const std::string& text);

#endif