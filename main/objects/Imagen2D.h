#ifndef IMAGEN2D_H
#define IMAGEN2D_H
#include "objects/Elemento2D.h"
#include "animation/Flipbook.h"   // Flipbook + FlipbookPlayer (el flipbook UNIFICADO del Core)

// =====================================================================
//  Imagen2D — un elemento de IMAGEN de una interfaz 2D: una textura
//  dentro del rectangulo del elemento, con modo de ajuste.
//
//  Si tiene un FLIPBOOK enganchado (flipPlay.fb != NULL) el MOTOR anima la UV del
//  quad: UpdateFlipbooks (registro unico del Core, Flipbook.cpp) avanza el tiempo del
//  player y el render dibuja la ventana UV actual. Lua no interviene por frame. Es el
//  MISMO Flipbook que usan Mesh y particulas: un solo sistema.
// =====================================================================
class Imagen2D : public Elemento2D {
public:
    std::string textura;  // ruta del archivo de imagen ("" = sin textura: rect gris)
    // como se acomoda la TEXTURA dentro del rectangulo del elemento:
    // 0 = estirar (deforma para llenar), 1 = ajustar (entera, con bandas),
    // 2 = cover (llena el rect recortando lo que sobra)
    int   modo;
    float color[4];       // TINTE de la textura (blanco = tal cual)
    int   palTinte;       // indice en la paleta del UI (-1 = tinte propio)
    bool  usarAlpha;      // false: ignora el canal alpha de la textura (se dibuja opaca)
    bool  filtrado;       // false: sin filtro (NEAREST, pixel-perfect)
    // SUB-RECT de la textura para la imagen FIJA (u0, v0, u1, v1; default
    // 0,0,1,1 = entera): con el ATLAS UNICO el HUD muestra solo su recorte.
    // Con flipbook enganchado manda la ventana del flipbook (que tiene su
    // propio origen tiraU0/tiraV0), no este rect.
    float uvRect[4];

    // --- FLIPBOOK (asset propio por ahora; en el futuro seran assets con nombre compartidos) ---
    Flipbook       flip;       // config: atlas/cols/filas/cuadros/fps + las 8 curvas de UV del quad
    FlipbookPlayer flipPlay;   // estado de reproduccion (fb == NULL => imagen fija, sin animar)

    Imagen2D(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Elemento2D(parent, "Imagen", pos) {
        modo = 0;
        ancho = 200.0f; alto = 200.0f;   // al elegir textura toma el tamano real del archivo
        color[0] = color[1] = color[2] = color[3] = 1.0f;
        usarAlpha = true;
        palTinte = -1;
        filtrado = true;
        uvRect[0] = 0.0f; uvRect[1] = 0.0f; uvRect[2] = 1.0f; uvRect[3] = 1.0f;
    }

    // engancha/desengancha el flipbook desde los campos de config (lo llaman el loader del
    // .w3dui y el binding setFlipbook). cuadros<=0 = imagen fija. El atlas es la propia textura.
    // ancho/alto/u0/v0: la SUB-TIRA dentro del atlas unico (defaults = textura entera).
    void FlipbookConfig(int cuadros, float fps, int cols, int filas, int desfase,
                        float tAncho = 1.0f, float tAlto = 1.0f,
                        float tU0 = 0.0f, float tV0 = 0.0f) {
        if (cuadros > 0) {
            flip.ConfigurarTira(textura, cols, filas, cuadros, fps, tAncho, tAlto, tU0, tV0);
            flipPlay.Set(&flip, desfase);
        } else {
            flipPlay.Set(0, 0);          // desengancha (sale del registro)
        }
    }
    bool EsFlipbook() const { return flipPlay.fb != 0; }

    ObjectType getType() W3D_OVERRIDE { return ObjectType::imagen2d; }
    // (sin destructor propio: ~FlipbookPlayer se desregistra solo del registro global)
};
#endif
