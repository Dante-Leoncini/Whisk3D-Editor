#include "ViewPorts/WithBorder.h"

// UVs e indices
GLubyte indicesBorder[] = {
    0,1, 4, 1,4, 5,   1, 2, 5, 5, 2, 6,   2, 3, 6, 6, 3, 7,    
    4,5, 8, 8,5, 9,                       6, 7,10,10, 7,11,
    8,9,12,12,9,13,   9,10,13,13,10,14,  10,11,14,14,11,15
};

GLfloat bourderUV[32] = {
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

void CalcBorderUV(int texW, int texH) {
    GLfloat* uv = bourderUV;

    // Coordenadas UV en píxeles (borde de 13px, esquinas de 6px, centro de 1px)
    float U[4] = { 115.0f / texW, 121.0f / texW, 122.0f / texW, 128.0f / texW };
    float V[4] = { 115.0f / texH, 121.0f / texH, 122.0f / texH, 128.0f / texH };

    // Generar los 16 pares UV (fila × columna)
    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            uv[k++] = U[x];
            uv[k++] = V[y];
        }
    }
}

// ------------------ Dibujar ------------------
void WithBorder::DibujarBordes(ViewportBase* current) {
    if (current == viewPortActive)
        glColor4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
    else
        glColor4f(ListaColores[static_cast<int>(ColorID::negro)][0], ListaColores[static_cast<int>(ColorID::negro)][1],
                  ListaColores[static_cast<int>(ColorID::negro)][2], ListaColores[static_cast<int>(ColorID::negro)][3]);

    glTexCoordPointer(2, GL_FLOAT, 0, bourderUV);
    glVertexPointer(2, GL_SHORT, 0, borderMesh);
    glDrawElements(GL_TRIANGLES, 48, GL_UNSIGNED_BYTE, indicesBorder);
}

// ------------------ Resize ------------------
void WithBorder::ResizeBorder(int width, int height) {
    GLshort U[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(width - 6*GlobalScale), (GLshort)(width) };
    GLshort V[4] = { 0, (GLshort)(6*GlobalScale), (GLshort)(height - 6*GlobalScale), (GLshort)(height) };

    int k = 0;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            borderMesh[k++] = U[x];
            borderMesh[k++] = V[y];
        }
    }
}