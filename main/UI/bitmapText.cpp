#include "bitmapText.h"

void RenderBitmapText(const std::string& text, textAlign align, int maxPixels){
    //glBindTexture(GL_TEXTURE_2D, WhiskFont->getTexture());
    glVertexPointer(2, GL_SHORT, 0, WhiskFont->getMesh());

    const int cw = CharacterWidthGS;
    int maxChars = maxPixels / cw;
    if (maxChars <= 0) return;

    // 1) contar caracteres UTF-8
    size_t i = 0;
    int totalChars = 0;
    while (i < text.size()) {
        UTF8_Char(text.c_str(), i);
        totalChars++;
    }

    int drawChars = totalChars > maxChars ? maxChars : totalChars;

    // 2) si es right, saltar los que no entran
    size_t start = 0;
    if (align == textAlign::right && totalChars > drawChars) {
        int skip = totalChars - drawChars;
        size_t j = 0;
        while (skip--) {
            UTF8_Char(text.c_str(), j);
        }
        start = j;
    }

    glPushMatrix();

    // 3) mover cursor inicial SOLO UNA VEZ
    if (align == textAlign::right) {
        glTranslatef(-drawChars * cw, 0, 0);
    }

    // 4) dibujar NORMAL, siempre hacia adelante
    i = start;
    int drawn = 0;
    while (i < text.size() && drawn < drawChars) {
        uint16_t cp = UTF8_Char(text.c_str(), i);

        if (cp != 0x20) {
            glTexCoordPointer(2, GL_FLOAT, 0, WhiskFont->getUV(cp));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        glTranslatef(cw, 0, 0);
        drawn++;
    }

    glPopMatrix();
}

void RenderBitmapFloat(float value, textAlign align, int maxPixels){
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4) << value;
    RenderBitmapText(ss.str(), align, maxPixels);
}