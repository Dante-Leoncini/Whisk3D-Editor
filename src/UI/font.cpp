#include "UI/font.h"

// Constructor
FontUV::FontUV(int texW, int texH) {
    // Letras mayúsculas
    addGlyph("A", texW, texH, 1, 0, 5, 11);
    addGlyph("B", texW, texH, 7, 0, 5, 11);
    addGlyph("C", texW, texH, 13, 0, 5, 11);
    addGlyph("D", texW, texH, 19, 0, 5, 11);
    addGlyph("E", texW, texH, 25, 0, 5, 11);
    addGlyph("F", texW, texH, 31, 0, 5, 11);
    addGlyph("G", texW, texH, 37, 0, 5, 11);
    addGlyph("H", texW, texH, 43, 0, 5, 11);
    addGlyph("I", texW, texH, 49, 0, 5, 11);
    addGlyph("J", texW, texH, 55, 0, 5, 11);
    addGlyph("K", texW, texH, 61, 0, 5, 11);
    addGlyph("L", texW, texH, 67, 0, 5, 11);
    addGlyph("M", texW, texH, 73, 0, 5, 11);
    addGlyph("N", texW, texH, 1, 10, 5, 11);
    addGlyph("Ñ", texW, texH, 79, 10, 5, 11);
    addGlyph("O", texW, texH, 7, 10, 5, 11);
    addGlyph("P", texW, texH, 13, 10, 5, 11);
    addGlyph("Q", texW, texH, 19, 10, 5, 11);
    addGlyph("R", texW, texH, 25, 10, 5, 11);
    addGlyph("S", texW, texH, 31, 10, 5, 11);
    addGlyph("T", texW, texH, 37, 10, 5, 11);
    addGlyph("U", texW, texH, 43, 10, 5, 11);
    addGlyph("V", texW, texH, 49, 10, 5, 11);
    addGlyph("W", texW, texH, 55, 10, 5, 11);
    addGlyph("X", texW, texH, 61, 10, 5, 11);
    addGlyph("Y", texW, texH, 67, 10, 5, 11);
    addGlyph("Z", texW, texH, 73, 10, 5, 11);

    // Letras minúsculas
    addGlyph("a", texW, texH, 1, 20, 5, 11);
    addGlyph("b", texW, texH, 7, 20, 5, 11);
    addGlyph("c", texW, texH, 13, 20, 5, 11);
    addGlyph("d", texW, texH, 19, 20, 5, 11);
    addGlyph("e", texW, texH, 25, 20, 5, 11);
    addGlyph("f", texW, texH, 31, 20, 5, 11);
    addGlyph("g", texW, texH, 37, 20, 5, 11);
    addGlyph("h", texW, texH, 43, 20, 5, 11);
    addGlyph("i", texW, texH, 49, 20, 5, 11);
    addGlyph("j", texW, texH, 55, 20, 5, 11);
    addGlyph("k", texW, texH, 61, 20, 5, 11);
    addGlyph("l", texW, texH, 67, 20, 5, 11);
    addGlyph("m", texW, texH, 73, 20, 5, 11);
    addGlyph("n", texW, texH, 1, 31, 5, 11);
    addGlyph("ñ", texW, texH, 79, 31, 5, 11);
    addGlyph("o", texW, texH, 7, 31, 5, 11);
    addGlyph("p", texW, texH, 13, 31, 5, 11);
    addGlyph("q", texW, texH, 19, 31, 5, 11);
    addGlyph("r", texW, texH, 25, 31, 5, 11);
    addGlyph("s", texW, texH, 31, 31, 5, 11);
    addGlyph("t", texW, texH, 37, 31, 5, 11);
    addGlyph("u", texW, texH, 43, 31, 5, 11);
    addGlyph("v", texW, texH, 49, 31, 5, 11);
    addGlyph("w", texW, texH, 55, 31, 5, 11);
    addGlyph("x", texW, texH, 61, 31, 5, 11);
    addGlyph("y", texW, texH, 67, 31, 5, 11);
    addGlyph("z", texW, texH, 73, 31, 5, 11);

    // Números y símbolos
    addGlyph("0", texW, texH, 1, 42, 5, 11);
    addGlyph("1", texW, texH, 7, 42, 5, 11);
    addGlyph("2", texW, texH, 13, 42, 5, 11);
    addGlyph("3", texW, texH, 19, 42, 5, 11);
    addGlyph("4", texW, texH, 25, 42, 5, 11);
    addGlyph("5", texW, texH, 31, 42, 5, 11);
    addGlyph("6", texW, texH, 37, 42, 5, 11);
    addGlyph("7", texW, texH, 43, 42, 5, 11);
    addGlyph("8", texW, texH, 49, 42, 5, 11);
    addGlyph("9", texW, texH, 55, 42, 5, 11);
    addGlyph("+", texW, texH, 61, 42, 5, 11);
    addGlyph("-", texW, texH, 67, 42, 5, 11);
    addGlyph("=", texW, texH, 73, 42, 5, 11);
    addGlyph("(", texW, texH, 1, 51, 5, 11);
    addGlyph(")", texW, texH, 7, 51, 5, 11);
    addGlyph("[", texW, texH, 13, 51, 5, 11);
    addGlyph("]", texW, texH, 19, 51, 5, 11);
    addGlyph("{", texW, texH, 25, 51, 5, 11);
    addGlyph("}", texW, texH, 31, 51, 5, 11);
    addGlyph("<", texW, texH, 37, 51, 5, 11);
    addGlyph(">", texW, texH, 43, 51, 5, 11);
    addGlyph("/", texW, texH, 49, 51, 5, 11);
    addGlyph("*", texW, texH, 55, 51, 5, 11);
    addGlyph(":", texW, texH, 61, 51, 5, 11);
    addGlyph("#", texW, texH, 67, 51, 5, 11);
    addGlyph("%", texW, texH, 73, 51, 5, 11);
    addGlyph("!", texW, texH, 1, 63, 5, 11);
    addGlyph("?", texW, texH, 7, 63, 5, 11);
    addGlyph(".", texW, texH, 13, 63, 5, 11);
    addGlyph(",", texW, texH, 19, 63, 5, 11);
    addGlyph("'", texW, texH, 25, 63, 5, 11);
    addGlyph("\"", texW, texH, 31, 63, 5, 11);
    addGlyph("@", texW, texH, 37, 63, 5, 11);
    addGlyph("&", texW, texH, 43, 63, 5, 11);
    addGlyph("$", texW, texH, 49, 63, 5, 11);
    addGlyph("¡", texW, texH, 55, 63, 5, 11);
}

// Devuelve las UV de un carácter
const GLfloat* FontUV::getUV(const std::string& c) {
    return atlas[c].uvs;
}

// Función privada para agregar glifo
void FontUV::addGlyph(const std::string& c, int texW, int texH, int x, int y, int w, int h) {
    GLfloat u1 = (GLfloat)x / texW;
    GLfloat u2 = (GLfloat)(x + w) / texW;
    GLfloat v1 = (GLfloat)y / texH;
    GLfloat v2 = (GLfloat)(y + h) / texH;

    Glyph g;
    g.uvs[0] = u1; g.uvs[1] = v1; // top-left
    g.uvs[2] = u2; g.uvs[3] = v1; // top-right
    g.uvs[4] = u1; g.uvs[5] = v2; // bottom-left
    g.uvs[6] = u2; g.uvs[7] = v2; // bottom-right

    atlas[c] = g;
}

// Instancia global
FontUV font(128, 128);