#include "font.h"

// ================= UTF-8 helpers =================

uint16_t UTF8_Char(const char* s, size_t& i) {
    unsigned char c = (unsigned char)s[i];

    if (c < 0x80) {
        i += 1;
        return c;
    }

    // UTF-8 de 2 bytes (suficiente para español)
    uint16_t cp = ((c & 0x1F) << 6) |
                  ((unsigned char)s[i + 1] & 0x3F);
    i += 2;
    return cp;
}



void Font::SetScale(GLshort scale){
    /*GLshort mesh[8] = {
        0,  0,
        5, 0,
        0, 5,
        5, 5
    };   */ 
    mesh[2] = mesh[6] = 5 * scale;
    mesh[5] = mesh[7] = 11 * scale;
}

// =================================================

Font::Font(int texW, int texH, GLuint textureID)
    : FontTexture(textureID),
      texWidth(texW),
      texHeight(texH)
{
    // Fallback = signo ?
    addGlyph('?', 7, 63, 5, 11);
    fallback = atlas['?'];

    // ===== MAYÚSCULAS =====
    addGlyph('A', 1, 0, 5, 11);
    addGlyph('B', 7, 0, 5, 11);
    addGlyph('C', 13, 0, 5, 11);
    addGlyph('D', 19, 0, 5, 11);
    addGlyph('E', 25, 0, 5, 11);
    addGlyph('F', 31, 0, 5, 11);
    addGlyph('G', 37, 0, 5, 11);
    addGlyph('H', 43, 0, 5, 11);
    addGlyph('I', 49, 0, 5, 11);
    addGlyph('J', 55, 0, 5, 11);
    addGlyph('K', 61, 0, 5, 11);
    addGlyph('L', 67, 0, 5, 11);
    addGlyph('M', 73, 0, 5, 11);
    addGlyph('N', 1, 10, 5, 11);
    addGlyph(0x00D1, 79, 0, 5, 11); // Ñ
    addGlyph('O', 7, 10, 5, 11);
    addGlyph('P', 13, 10, 5, 11);
    addGlyph('Q', 19, 10, 5, 11);
    addGlyph('R', 25, 10, 5, 11);
    addGlyph('S', 31, 10, 5, 11);
    addGlyph('T', 37, 10, 5, 11);
    addGlyph('U', 43, 10, 5, 11);
    addGlyph('V', 49, 10, 5, 11);
    addGlyph('W', 55, 10, 5, 11);
    addGlyph('X', 61, 10, 5, 11);
    addGlyph('Y', 67, 10, 5, 11);
    addGlyph('Z', 73, 10, 5, 11);
    addGlyph('_', 79, 10, 5, 11);

    // ===== MINÚSCULAS =====
    addGlyph('a', 1, 20, 5, 11);
    addGlyph('b', 7, 20, 5, 11);
    addGlyph('c', 13, 20, 5, 11);
    addGlyph('d', 19, 20, 5, 11);
    addGlyph('e', 25, 20, 5, 11);
    addGlyph('f', 31, 20, 5, 11);
    addGlyph('g', 37, 20, 5, 11);
    addGlyph('h', 43, 20, 5, 11);
    addGlyph('i', 49, 20, 5, 11);
    addGlyph('j', 55, 20, 5, 11);
    addGlyph('k', 61, 20, 5, 11);
    addGlyph('l', 67, 20, 5, 11);
    addGlyph('m', 73, 20, 5, 11);
    addGlyph('n', 1, 31, 5, 11);
    addGlyph(0x00F1, 79, 31, 5, 11); // ñ
    addGlyph('o', 7, 31, 5, 11);
    addGlyph('p', 13, 31, 5, 11);
    addGlyph('q', 19, 31, 5, 11);
    addGlyph('r', 25, 31, 5, 11);
    addGlyph('s', 31, 31, 5, 11);
    addGlyph('t', 37, 31, 5, 11);
    addGlyph('u', 43, 31, 5, 11);
    addGlyph('v', 49, 31, 5, 11);
    addGlyph('w', 55, 31, 5, 11);
    addGlyph('x', 61, 31, 5, 11);
    addGlyph('y', 67, 31, 5, 11);
    addGlyph('z', 73, 31, 5, 11);

    // ===== SÍMBOLOS =====
    addGlyph('0', 1, 42, 5, 11);
    addGlyph('1', 7, 42, 5, 11);
    addGlyph('2', 13, 42, 5, 11);
    addGlyph('3', 19, 42, 5, 11);
    addGlyph('4', 25, 42, 5, 11);
    addGlyph('5', 31, 42, 5, 11);
    addGlyph('6', 37, 42, 5, 11);
    addGlyph('7', 43, 42, 5, 11);
    addGlyph('8', 49, 42, 5, 11);
    addGlyph('9', 55, 42, 5, 11);

    addGlyph('+', 61, 42, 5, 11); 
    addGlyph('-', 67, 42, 5, 11); 
    addGlyph('=', 73, 42, 5, 11); 
    addGlyph('(', 1, 52, 5, 11); 
    addGlyph(')', 7, 52, 5, 11); 
    addGlyph('[', 13, 52, 5, 11); 
    addGlyph(']', 19, 52, 5, 11); 
    addGlyph('{', 25, 52, 5, 11);
    addGlyph('}', 31, 52, 5, 11); 
    addGlyph('<', 37, 52, 5, 11); 
    addGlyph('>', 43, 52, 5, 11); 
    addGlyph('/', 49, 52, 5, 11);
    addGlyph('*', 55, 52, 5, 11); 
    addGlyph(':', 61, 52, 5, 11); 
    addGlyph('#', 67, 52, 5, 11); 
    addGlyph('%', 73, 52, 5, 11); 
    addGlyph('!', 1, 64, 5, 11); 
    addGlyph('?', 7, 64, 5, 11); 
    addGlyph('.', 13, 63, 5, 11); 
    addGlyph(',', 19, 63, 5, 11); 
    addGlyph('\'', 25, 64, 5, 11);
    addGlyph('\"', 31, 64, 5, 11); 
    addGlyph('@', 37, 64, 5, 11); 
    addGlyph('&', 43, 64, 5, 11); 
    addGlyph('$', 49, 64, 5, 11); 
    addGlyph(0x00A1, 55, 64, 5, 11); // ¡
    addGlyph(0x00B0, 61, 64, 5, 11); // °
    addGlyph('|', 67, 64, 5, 11);
}

// ================= API =================

const GLfloat* Font::getUV(uint16_t codepoint) const {
    auto it = atlas.find(codepoint);
    if (it == atlas.end())
        return fallback.uvs;
    return it->second.uvs;
}

const GLshort* Font::getMesh() const {
    return mesh;
}

// ================= INTERNAL =================

void Font::addGlyph(uint16_t codepoint, int x, int y, int w, int h) {
    Glyph g;

    GLfloat u1 = (GLfloat)x / texWidth;
    GLfloat v1 = (GLfloat)y / texHeight;
    GLfloat u2 = (GLfloat)(x + w) / texWidth;
    GLfloat v2 = (GLfloat)(y + h) / texHeight;

    g.uvs[0] = u1; g.uvs[1] = v1;
    g.uvs[2] = u2; g.uvs[3] = v1;
    g.uvs[4] = u1; g.uvs[5] = v2;
    g.uvs[6] = u2; g.uvs[7] = v2;

    atlas[codepoint] = g;
}

// ================= GLOBAL =================

Font* WhiskFont = nullptr;