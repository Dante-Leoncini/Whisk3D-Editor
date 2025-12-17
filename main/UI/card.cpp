#include "card.h"

// UVs e indices
GLubyte CardIndices[] = {
    0,1, 4, 1,4, 5,   1, 2, 5, 5, 2,  6,   2, 3, 6, 6, 3, 7,    
    4,5, 8, 8,5, 9,   5, 6, 9, 6, 9, 10,   6, 7,10,10, 7,11,
    8,9,12,12,9,13,   9,10,13,13,10, 14,  10,11,14,14,11,15
};

GLubyte CardBorderIndices[] = {
    0,1, 4, 1,4, 5,   1, 2, 5, 5, 2,  6,   2, 3, 6, 6, 3, 7,    
    4,5, 8, 8,5, 9,                        6, 7,10,10, 7,11,
    8,9,12,12,9,13,   9,10,13,13,10, 14,  10,11,14,14,11,15
};

GLfloat CardUV[32] = {
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f
};

GLfloat CardBordeUV[32] = {
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f,
    0.0f,      0.0f
};

void CalcCardUV(int texW, int texH) {
    GLfloat* uv = CardUV;

    // Coordenadas UV en píxeles (borde de 13px, esquinas de 6px, centro de 1px)
    float U[4] = { 122.0f / texW, 124.0f / texW, 125.0f / texW, 127.0f / texW };
    float V[4] = { 103.0f / texH, 105.0f / texH, 106.0f / texH, 108.0f / texH };

    // Generar los 16 pares UV (fila × columna)
    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            uv[k++] = U[x];
            uv[k++] = V[y];
        }
    }

    GLfloat* uvBorder = CardBordeUV;

    // Coordenadas UV en píxeles (borde de 13px, esquinas de 6px, centro de 1px)
    float Ub[4] = { 116.0f / texW, 118.0f / texW, 119.0f / texW, 121.0f / texW };
    float Vb[4] = { 103.0f / texH, 105.0f / texH, 106.0f / texH, 108.0f / texH };

    // Generar los 16 pares UV (fila × columna)
    k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            uvBorder[k++] = Ub[x];
            uvBorder[k++] = Vb[y];
        }
    }
}

// Constructor
Card::Card(Object2D* parent, int w, int h) : Object2D(parent) {
    Resize(w, h);
}

// ------------------ Resize ------------------
void Card::Resize(int width, int height) {
    GLshort U[4] = { 0, (GLshort)(2*GlobalScale), (GLshort)(width - 2*GlobalScale), (GLshort)(width) };
    GLshort V[4] = { 0, (GLshort)(2*GlobalScale), (GLshort)(height - 2*GlobalScale), (GLshort)(height) };

    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            mesh[k++] = U[x];
            mesh[k++] = V[y];
        }
    }
}

// Cambia el color del rectángulo
void Card::SetColor(GLubyte red, GLubyte green, GLubyte blue) {
    color[0] = red;
    color[1] = green;
    color[2] = blue;
}

// Renderiza el rectángulo
void Card::RenderObject(bool usarColorPropio) {
    if (usarColorPropio)
        glColor4ub(color[0], color[1], color[2], opacity);

    glTexCoordPointer(2, GL_FLOAT, 0, CardUV);
    glVertexPointer(2, GL_SHORT, 0, mesh);
    glDrawElements(GL_TRIANGLES, 54, GL_UNSIGNED_BYTE, CardIndices);
}

// Renderiza el rectángulo
void Card::RenderBorder(bool usarColorPropio) {
    if (usarColorPropio)
        glColor4ub(color[0], color[1], color[2], opacity);

    glTexCoordPointer(2, GL_FLOAT, 0, CardBordeUV);
    glVertexPointer(2, GL_SHORT, 0, mesh);
    glDrawElements(GL_TRIANGLES, 48, GL_UNSIGNED_BYTE, CardBorderIndices);
}