#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "ViewPorts/WithBorder.h"
#include "objects/Textures.h" // Textures[0] = atlas de iconos (el borde es texturizado)
#ifdef W3D_SYMBIAN
    #include "ui/W3dColors.h"
// UVs expandidos por indice para drawArrays (se llenan en CalcBorderUV)
GLfloat bourderUVExp[96];
#endif

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
#ifdef W3D_SYMBIAN
    for (int i = 0; i < 48; i++) {
        bourderUVExp[i*2 + 0] = bourderUV[indicesBorder[i]*2 + 0];
        bourderUVExp[i*2 + 1] = bourderUV[indicesBorder[i]*2 + 1];
    }
#endif
}

// C++03: el mesh default (18x18) se arma aca, no como init de clase
WithBorder::WithBorder() {
    static const GLshort KDefault[32] = {
        0,0,   6,0,   12,0,   18,0,
        0,6,   6,6,   12,6,   18,6,
        0,12,  6,12,  12,12,  18,12,
        0,18,  6,18,  12,18,  18,18
    };
    for (int i = 0; i < 32; i++) { borderMesh[i] = KDefault[i]; }
#ifdef W3D_SYMBIAN
    for (int i = 0; i < 48; i++) {
        borderMeshExp[i*2 + 0] = (GLfloat)borderMesh[indicesBorder[i]*2 + 0];
        borderMeshExp[i*2 + 1] = (GLfloat)borderMesh[indicesBorder[i]*2 + 1];
    }
#endif
}

// ------------------ Dibujar ------------------
void WithBorder::DibujarBordes(ViewportBase* current) {
#ifdef W3D_SYMBIAN
    // misma geometria y arte que PC, pero con drawArrays + vertices FLOAT
    // expandidos (glDrawElements/SHORT no dibujan en la fase 2D del N95)
    const float* c = (current == viewPortActive)
        ? ListaColores[static_cast<int>(ColorID::accent)] : ListaColores[static_cast<int>(ColorID::negro)];
    w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    w3dEngine::TexCoordPointer2f(0, bourderUVExp);
    w3dEngine::VertexPointer2f(0, borderMeshExp);
    w3dEngine::DrawTrianglesArray(48);
#else
    // el borde es TEXTURIZADO (arte del atlas de iconos, con alpha). Hay que dejar TODO el estado que
    // necesita, IGUAL que el UV editor: Texture2D + Blend + arrays de VERTICE y TexCoord habilitados +
    // el atlas (Textures[0]) bindeado. Sin VertexArray habilitado el VertexPointer no se usa (nada
    // dibuja); sin bindear el atlas toma la textura del mesh del viewport y sale invisible en ES2/WebGL.
    // el borde va en el BORDE del viewport: si quedo un scissor del render 3D (recorta al area
    // interior), el marco se ve "recortado" / no se ve. Lo apagamos (como el UV editor).
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend); w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::ColorArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    if (!Textures.empty() && Textures[0]) w3dEngine::BindTexture(Textures[0]->iID);

    if (current == viewPortActive)
        w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::accent)][0], ListaColores[static_cast<int>(ColorID::accent)][1],
                  ListaColores[static_cast<int>(ColorID::accent)][2], ListaColores[static_cast<int>(ColorID::accent)][3]);
    else
        w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::negro)][0], ListaColores[static_cast<int>(ColorID::negro)][1],
                  ListaColores[static_cast<int>(ColorID::negro)][2], ListaColores[static_cast<int>(ColorID::negro)][3]);

    w3dEngine::TexCoordPointer2f(0, bourderUV);
    w3dEngine::VertexPointer2s(0, borderMesh);
    w3dEngine::DrawTrianglesByte(48, indicesBorder);
#endif
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
#ifdef W3D_SYMBIAN
    for (int i = 0; i < 48; i++) {
        borderMeshExp[i*2 + 0] = (GLfloat)borderMesh[indicesBorder[i]*2 + 0];
        borderMeshExp[i*2 + 1] = (GLfloat)borderMesh[indicesBorder[i]*2 + 1];
    }
#endif
}