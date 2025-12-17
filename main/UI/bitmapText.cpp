#include "bitmapText.h"

void RenderBitmapText(const std::string& text){
    glBindTexture(GL_TEXTURE_2D, WhiskFont->getTexture());
    glVertexPointer(2, GL_SHORT, 0, WhiskFont->getMesh());

    size_t i = 0;
    while (i < text.size()) {
        uint16_t cp = UTF8_Char(text.c_str(), i);

        // ESPACIO → solo avanzar cursor
        if (cp == 0x20) {
            glTranslatef(6 * GlobalScale, 0, 0);
            continue;
        }

        glTexCoordPointer(2, GL_FLOAT, 0, WhiskFont->getUV(cp));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glTranslatef(6 * GlobalScale, 0, 0);
    }
}
