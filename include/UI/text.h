#ifndef TEXT_H
#define TEXT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <string>
#include <GL/gl.h>

#include "UI/UI.h"
#include "UI/font.h"
#include "UI/sprites.h"
#include "UI/Object2D.h"
#include "OpcionesRender.h"
#include "Objects/Textures.h"

class Text : public Object2D {
public:
    int interpolacion = closest;
    std::string value;
    std::vector<Sprite> letters;

    Text(const std::string& nombre = "Text", Object2D* parent = nullptr);

    void SetColor(GLubyte red, GLubyte green, GLubyte blue);
    void SetValue(const std::string& newValue);
    void SetScaleX(int value);
    void SetScaleY(int value);
    void SetX(int input);
    void SetY(int input);

    void UpdateCache(); // recalcula posiciones y UVs
    void RenderObject(bool UsarColor = true) override;
};

#endif