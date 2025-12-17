#ifndef FONT_H
#define FONT_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <map>
#include <cstdint>
#include <GL/gl.h>

#include "objects/Textures.h"

class Font {
    public:
        struct Glyph {
            GLfloat uvs[8];
        };

        Font(int texW, int texH, GLuint textureID);

        // Obtener UV por codepoint
        const GLfloat* getUV(uint16_t codepoint) const;

        // Malla compartida (quad)
        const GLshort* getMesh() const;

        GLuint getTexture() const { return FontTexture; }

        void SetScale(GLshort scale);

    private:
        void addGlyph(uint16_t codepoint, int x, int y, int w, int h);

        GLuint FontTexture;
        int texWidth;
        int texHeight;

        std::map<uint16_t, Glyph> atlas;

        GLshort mesh[8] = {
            0,  0,
            5, 0,
            0,  5,
            5, 5
        };

        Glyph fallback; // '?' o vacío
};


// ================= UTF-8 helper =================

uint16_t UTF8_Char(const char* s, size_t& i);

// ================= GLOBAL =================

extern Font* WhiskFont;

#endif
