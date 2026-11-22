#ifndef IMAGE2_D_H
#define IMAGE2_D_H
#include "objects/Element2D.h"
#include "animation/Flipbook.h"   // Flipbook + FlipbookPlayer (el flipbook UNIFICADO del Core)

// =====================================================================
//  Image2D — un elemento de IMAGEN de una interfaz 2D: una textura
//  dentro del rectangulo del elemento, con modo de ajuste.
//
//  Si tiene un FLIPBOOK enganchado (flipPlay.fb != NULL) el MOTOR anima la UV del
//  quad: UpdateFlipbooks (registro unico del Core, Flipbook.cpp) avanza el tiempo del
//  player y el render dibuja la ventana UV actual. Lua no interviene por frame. Es el
//  MISMO Flipbook que usan Mesh y particulas: un solo sistema.
// =====================================================================
class Image2D : public Element2D {
public:
    std::string texture;  // ruta del archivo de imagen ("" = sin textura: rect gris)
    // como se acomoda la TEXTURA dentro del rectangulo del elemento:
    // 0 = estirar (deforma para llenar), 1 = ajustar (entera, con bandas),
    // 2 = cover (llena el rect recortando lo que sobra)
    int   mode;
    float color[4];      // TINTE de la textura (blanco = tal cual)
    int   palTint;       // indice en la paleta del UI (-1 = tinte propio)
    bool  useAlpha;      // false: ignora el canal alpha de la textura (se dibuja opaca)
    bool  filtered;      // false: sin filtro (NEAREST, pixel-perfect)
    int   mix;           // w3dEngine::Mezcla (Alpha por defecto; Add/AddAlpha para brillos y destellos de un HUD)
    // SUB-RECT de la textura para la imagen FIJA (u0, v0, u1, v1; default
    // 0,0,1,1 = entera): con el ATLAS UNICO el HUD muestra solo su recorte.
    // Con flipbook enganchado manda la ventana del flipbook (que tiene su
    // propio origen stripU0/stripV0), no este rect.
    float uvRect[4];

    // --- FLIPBOOK (asset propio por ahora; en el futuro seran assets con nombre compartidos) ---
    Flipbook       flip;       // config: atlas/cols/filas/cuadros/fps + las 8 curvas de UV del quad
    FlipbookPlayer flipPlay;   // estado de reproduccion (fb == NULL => imagen fija, sin animar)

    Image2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Element2D(parent, "Image", pos) {
        mode = 0;
        width = 200.0f; height = 200.0f;   // al elegir textura toma el tamano real del archivo
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        useAlpha = true;
        palTint  = -1;
        filtered = true;
        mix = 1;   // w3dEngine::MixAlpha
        uvRect[0] = 0.0f; uvRect[1] = 0.0f; uvRect[2] = 1.0f; uvRect[3] = 1.0f;
    }

    // engancha/desengancha el flipbook desde los campos de config (lo llaman el loader del
    // .w3dui y el binding setFlipbook). cuadros<=0 = imagen fija. El atlas es la propia textura.
    // ancho/alto/u0/v0: la SUB-TIRA dentro del atlas unico (defaults = textura entera).
    void FlipbookConfig(int frames, float fps, int cols, int rows, int offset,
                        float tWidth = 1.0f, float tHeight = 1.0f,
                        float tU0 = 0.0f, float tV0 = 0.0f) {
        if (frames > 0) {
            flip.ConfigureStrip(texture, cols, rows, frames, fps, tWidth, tHeight, tU0, tV0);
            flipPlay.Set(&flip, offset);
        } else {
            flipPlay.Set(0, 0);          // desengancha (sale del registro)
        }
    }
    bool IsFlipbook() const { return flipPlay.fb != 0; }

    ObjectType getType() W3D_OVERRIDE { return ObjectType::image2d; }
    // (sin destructor propio: ~FlipbookPlayer se desregistra solo del registro global)
};
#endif
