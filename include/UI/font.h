#ifndef FONT_H
#define FONT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <map>
#include <string>
#include <GL/gl.h>

class FontUV {
public:
    struct Glyph {
        GLfloat uvs[8]; // cada letra tiene sus UV
    };

    std::map<std::string, Glyph> atlas; // diccionario de letras (clave: "A", "Ñ", etc.)

    FontUV(int texW, int texH);
    const GLfloat* getUV(const std::string& c);

private:
    void addGlyph(const std::string& c, int texW, int texH, int x, int y, int w, int h);
};

// Instancia global
extern FontUV font;

#endif