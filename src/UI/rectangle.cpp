#include "UI/rectangle.h"

// Constructor
Rec2D::Rec2D(Object2D* parent) : Object2D(parent) {
    // Inicialización de width, height y vertices ya se hace en la declaración
}

// Configura tamaño y recalcula vértices
void Rec2D::SetSize(GLshort x, GLshort y, GLshort w, GLshort h) {
    width = w;
    height = h;

    vertices[0] = x;
    vertices[1] = y;

    vertices[2] = x + w;
    vertices[3] = y;

    vertices[4] = x;
    vertices[5] = y + h;

    vertices[6] = x + w;
    vertices[7] = y + h;
}

// Cambia el color del rectángulo
void Rec2D::SetColor(GLubyte red, GLubyte green, GLubyte blue) {
    color[0] = red;
    color[1] = green;
    color[2] = blue;
}

// Renderiza el rectángulo
void Rec2D::RenderObject(bool usarColorPropio) {
    if (usarColorPropio)
        glColor4ub(color[0], color[1], color[2], opacity);

    glVertexPointer(2, GL_SHORT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}