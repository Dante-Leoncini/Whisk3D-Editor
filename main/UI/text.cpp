#include "text.h"

// Constructor
Text::Text(const std::string& nombre, Object2D* parent)
    : Object2D(parent) 
{
    SetValue(nombre);
}

// Set color del texto
void Text::SetColor(GLubyte red, GLubyte green, GLubyte blue){
    color[0] = red;
    color[1] = green;
    color[2] = blue;
}

// Setear texto y actualizar cache
void Text::SetValue(const std::string& newValue){
    value = newValue;
    UpdateCache();
}

// Setear escala X
void Text::SetScaleX(int value){
    scaleX = value;
    UpdateCache();
}

// Setear escala Y
void Text::SetScaleY(int value){
    scaleY = value;
    UpdateCache();
}

// Setear posición X
void Text::SetX(int input){
    int offsetX = input;

    for (auto& ls : letters) {
        ls.SetX(
            x + offsetX,
            5 * scaleX
        );
        offsetX += 6 * scaleX;
    }
}

// Setear posición Y
void Text::SetY(int input){
    int offsetY = input;

    for (auto& ls : letters) {
        ls.SetY(
            y + offsetY,
            11 * scaleY
        );
    }
}

// Recalcular posiciones y UVs
void Text::UpdateCache() {
    letters.clear();
    int offsetX = 0;
    size_t i = 0;

    while (i < value.size()) {
        std::string letra;
        unsigned char c = value[i];

        if (c < 128) { 
            letra = std::string(1, c); 
            i++; 
        } else { 
            letra = value.substr(i, 2); 
            i += 2; 
        }

        Sprite ls;
        const GLfloat* uv = getFont().getUV(letra);

        for (int j = 0; j < 8; ++j) ls.uvs[j] = uv[j];

        ls.SetVertices(
            x + offsetX,
            y,
            5 * scaleX * GlobalScale,
            11 * scaleY * GlobalScale
        );

        letters.push_back(ls);
        offsetX += 6 * scaleX * GlobalScale;
    }
}

// Renderizar texto
void Text::RenderObject(bool UsarColor) {
    if (UsarColor){ 
        glColor4ub(color[0], color[1], color[2], opacity);                
    }
    glBindTexture(GL_TEXTURE_2D, Textures[0].iID);

    if (interpolacion == closest){
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    for (const auto& ls : letters) {
        ls.Render();
    }
}