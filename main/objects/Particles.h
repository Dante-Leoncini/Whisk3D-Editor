#ifndef PARTICLES_H
#define PARTICLES_H
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner
#include "graphics/w3dParticles.h"   // el sistema de particulas del Core (billboards)
#include <string>

// ============================================================================
//  Objeto Particulas: el EMISOR 3D que envuelve al sistema de particulas del
//  Core (libs/Whisk3DCore/graphics/w3dParticles.h). El emisor vive en su transform:
//  las particulas NACEN en su origen de MUNDO, disparadas en un CONO alrededor
//  de su eje +Y local (rotar el objeto apunta el chorro), y despues viven en
//  espacio de MUNDO (no siguen al emisor: el polvo de una pisada queda atras).
//
//  Contrato del .w3d de texto (mismos nombres en el panel y el JSON v4):
//      Particulas {
//          textura: "textures/polvo.png"   // PNG con alpha (relativa al .w3d)
//          cantidad: 20        // particulas POR SEGUNDO (0 = solo rafagas emitir())
//          vida: 1.2           // segundos de vida de cada particula
//          tam: 0.4            // LADO del billboard en unidades de mundo
//          vel: 2.0            // velocidad inicial (unidades/seg)
//          dispersion: 30      // apertura TOTAL del cono, en grados (0 = chorro recto)
//          gravedad: 4         // aceleracion -Y de mundo (+ cae, - sube, 0 = flota)
//          aditivo: false      // true = mezcla ADITIVA (brillos); false = alpha comun
//          sustractivo: false  // true = mezcla SUSTRACTIVA dst-src (humo, polvo); gana sobre aditivo
//          color: "1, 1, 1, 1" // tinte r, g, b + alpha inicial
//          desvanecer: true    // alpha -> 0 a lo largo de la vida
//          activo: true        // false = no emite (ni el rate ni emitir()); las vivas terminan su vida
//          variacion: 0.3      // 0..1: jitter POR PARTICULA sobre vel, vida y tam (+-30%)
//          turbulencia: 1.5    // deriva azarosa SUAVE por particula (unidades/s^2; 0 = recta)
//          rotacion: true      // cada particula NACE con un angulo azaroso 0..360 y lo
//                              // mantiene (el "rotz fijo al nacer" del original). false
//                              // (default) = billboard derecho (angulo 0, deterministico)
//          velRotation: 90     // giro CONTINUO en grados/seg, signo azaroso por particula
//                              // (0 = sin giro). Independiente de 'rotacion'.
//      }
//
//  ORDEN DE ESCENA (documentado a proposito): las particulas son TRANSLUCIDAS,
//  asi que NO se dibujan durante el traversal del arbol. RenderObject solo ANOTA
//  el emisor en la lista del frame; el viewport las dibuja todas juntas DESPUES
//  de SceneCollection->Render() (todos los opacos ya estan, con su z-buffer) y
//  ANTES de los overlays del editor (huesos, motion trail, passepartout, UI).
//  Se dibujan con depth test PRENDIDO (se tapan detras de los muros) y depth
//  write APAGADO (entre ellas no se pisan), como billboards mirando a la camara
//  de la vista bindeada (DrawBillboard con g_renderCamRight/Up).
//
//  EL ORDEN COMPLETO DEL FRAME, Y POR QUE ES ESTE (el motor NO tiene un "pase
//  transparente" global que reordene objetos: entre objetos manda el ARBOL, y
//  `order_pass` solo ordena los grupos ADENTRO de una malla):
//
//     arbol de la escena (opacos y translucidos, en su orden)
//       -> CALCOMANIAS diferidas (sombras: van sobre la escena opaca terminada)
//       -> PARTICULAS  diferidas (lo ultimo del mundo 3D)
//       -> overlays del editor / HUD
//
//  Las particulas van ULTIMAS y no "despues de los opacos y antes de los alpha"
//  por una razon concreta: un emisor esta pegado a la entidad que lo tira y suele
//  quedar DELANTE de mallas translucidas que estan mas atras en el arbol (una
//  mascara, un vidrio, una lamina de agua). Dibujarlas antes que esas mallas
//  exigiria que las mallas no escriban z para no borrarlas; dibujarlas al final,
//  con z-test y SIN z-write, no le exige nada a nadie: se tapan detras de lo que
//  esta genuinamente delante y mezclan sobre lo que esta detras.
//
//  El corolario para las mallas: lo que es LUZ (mezcla aditiva/sustractiva/
//  multiplicativa/screen) tampoco escribe z aunque se dibuje en el arbol
//  (W3dMaterialIsLight, Materials.h). Sin eso, una "particula" hecha con quads
//  de malla le abre un agujero a lo que se dibuje despues y mas lejos.
//
//  El bind lua emitir(obj, n) dispara una RAFAGA de n particulas ya (el polvo
//  de las pisadas, un estallido puntual); con cantidad: 0 el emisor emite
//  SOLO por rafagas. El comando del harness es partinfo (config + vivas).
// ============================================================================
class Particles : public Object {
public:
    // ---- propiedades (texto + v4 + panel; ver el contrato de arriba) ----
    std::string texture;   // ruta del PNG (con alpha). "" = no se dibuja nada.
    float count;           // particulas por segundo (0 = solo rafagas)
    float life;            // segundos
    float size;            // lado del billboard (unidades de mundo)
    float vel;             // velocidad inicial (unidades/seg)
    float dispersion;      // apertura TOTAL del cono (grados)
    float gravity;         // aceleracion -Y de mundo (+ cae, - sube)
    // MODO DE MEZCLA (dropdown en el panel): un solo campo con el enum del Core w3dEngine::Mezcla
    // (Off/Alpha/Add/AddAlpha/Multiply/Screen/Premult/Subtract). Reemplaza a los viejos bool aditivo +
    // bool sustractivo (que no podian ser dos a la vez ni expresar Multiply/Screen). Default MixAlpha.
    // Subtract (dst - src) OSCURECE: sirve para humo gris sobre cielo claro (chimeneas), que ni alpha ni
    // aditiva logran. El Core ya soporta todos (REVERSE_SUBTRACT en PC/WebGL, aproximacion en GL ES 1.1 del N95).
    int   mix;           // w3dEngine::Mezcla
    float color[4];      // tinte r,g,b + alpha inicial
    bool  fade;          // alpha -> 0 con la vida
    bool  active;        // false = no emite nada (las vivas terminan su vida)
    // FILTRADO de la textura (checkbox del panel): true = LINEAR (suave), false = NEAREST (pixelada,
    // pixel-art). El pase de particulas lo aplica con TexFilter al bindear (como el material). Default
    // true: el fuego y el humo quieren bordes suaves. Igual que el resto del 2D, respeta el "modo
    // pixelado" global del editor (PixelatedGlobal).
    bool  filtered;
    // AZAR (feedback del dueno: "el juego original era mas azaroso"). Defaults 0 =
    // comportamiento exacto de antes (round-trip de escenas viejas intacto).
    float variation;      // 0..1: jitter POR PARTICULA sobre vel, vida y tam (0.3 = +-30%)
    float turbulence;     // deriva azarosa SUAVE por particula durante la vida (unidades/s^2)
    // ROTACION DEL BILLBOARD, girado en su plano (feedback del dueno: "me parece
    // que a las particulas le falta rotaciones"). El Core ya rota el quad por
    // p.rot/p.spin (DrawBillboard recibe right/up y los gira por particula); lo
    // que faltaba era CONTROLARLO desde el emisor: el Core sorteaba un rot
    // azaroso siempre y el emisor dejaba spin en 0. Ahora el emisor decide:
    bool  rotation;        // true = nace con angulo azaroso 0..360 FIJO (rotz del original);
                           // false (default) = nace derecha (angulo 0, deterministico)
    float velRotation;     // grados/seg de giro continuo, SIGNO azaroso por particula (0 = sin giro)
    // FLIPBOOK por EDAD (sin Lua): la textura es un atlas de flipCols x flipRows; cada particula
    // recorre flipFrames celdas segun su edad. flipFrames=0 = sin flipbook (textura entera). Lo
    // maneja el Core (ParticleSystem::CellUV); es el mismo concepto de flipbook que UI/Mesh.
    int   flipCols, flipRows, flipFrames;
    // RADIO DE EMISION (perf del N95, [PERF] part=8ms con emisores del otro lado del
    // mapa): mas lejos que esto de la camara el emisor NO emite (las vivas terminan
    // su vida y la sim queda vacia = gratis). 0 = sin limite. Default 45 u.
    float emissionRadius;
    // SUB-RECT del ATLAS UNICO (u0, v0, u1, v1; default 0,0,1,1 = la textura
    // entera): el sprite (y su flipbook) viven en ese rect. Con todos los
    // emisores sobre el mismo atlas el pase de particulas usa UN solo bind.
    float uvRect[4];

    // ================================================================================
    //  LO AVANZADO (todo opcional: con los defaults el emisor es exactamente el de antes).
    //  Unidades por SEGUNDO (independiente del framerate). Para traducir un efecto de juego que
    //  integra POR FRAME a 30 fps: un factor por frame k -> por segundo k^30.
    // ================================================================================
    // -- movimiento --
    float velLocal[3];      // velocidad BASE fija en ejes LOCALES del emisor (m/s), se suma a la del cono
    float velRandom[3];     // +- azar de velocidad POR EJE, en ejes LOCALES del emisor (m/s), ademas del cono
    float posRandom[3];     // +- azar de la POSICION de nacimiento por eje local (m): caja de nacimiento
    float acceleration[3];  // aceleracion propia en ejes LOCALES del emisor (m/s^2), fijada al nacer (mundo)
    float drag;             // la velocidad se multiplica por esto cada segundo (1 = sin arrastre; 0.08 = frena fuerte)
    // -- tamanio --
    float growth;     // el tamanio gana esto x (tamanio inicial) por segundo al nacer (0 = no crece)
    float brakeGrowth;// esa tasa se multiplica por esto cada segundo (1 = crece parejo; <1 = crece y se frena)
    // -- color --
    bool  useColorFinal;  // el color va de 'color' a 'colorFinal' a lo largo de la vida
    float colorFinal[4];  // rgb al morir + alpha al morir (multiplica al alpha inicial)
    float alphaDecays;    // el alpha se multiplica por esto cada segundo (1 = apagado; 0.0001 = muere rapido)
    float alphaDeath;     // muere cuando su alpha baja de esto (0..1; 0 = nunca por alpha)
    // -- rotacion (con 'rotacion' apagado) --
    float initialRot;       // angulo al nacer (grados)
    float rotRandom;        // +- azar del angulo al nacer (grados)
    float velRotRandom;     // +- azar del giro (grados/s)
    bool  turnSignRandom;   // true (default, como siempre) = el giro sale para un lado u otro al azar
    // -- fundidos de dibujo (no cambian cuando muere) --
    float appear;        // segundos de fundido de ENTRADA al nacer (0 = nace entera)
    float fadeNear[2];   // distancia a la camara: invisible hasta [0], entera desde [1] ([1] = 0: apagado)
    float fadeFar[2];    // entera hasta [0], invisible desde [1] ([1] = 0: apagado)
    // -- flipbook --
    float flipFps;       // cuadros por segundo (0 = el flipbook se reparte en la vida)
    bool  flipOnce;      // con flipFps: muere al terminar el ultimo cuadro
    // -- forma --
    int   shape;            // w3dEngine::ParticleShape: 0 billboard, 1 estirada (por la velocidad), 2 linea (estela)
    float stretch;          // estirada: largo = segundos de velocidad
    int   trailPoints;      // linea: puntos de la estela (2..8)
    float trailStep;        // linea: segundos entre puntos
    float thicknessLine;    // linea: ancho en pixeles

    // ---- runtime (no se guarda) ----
    w3dEngine::ParticleSystem sys; // el sistema del Core: simula y dibuja
    float emAcc;                   // acumulador de la emision continua (cantidad/seg)

    Particles(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Particles", pos) {
        count = 10.0f; life = 1.0f; size = 0.3f; vel = 1.5f;
        dispersion = 30.0f; gravity = 0.0f; mix = w3dEngine::MixAlpha;
        variation = 0.0f; turbulence = 0.0f;
        rotation = false; velRotation = 0.0f;   // billboards derechos y quietos (deterministico)
        flipCols = 1; flipRows = 1; flipFrames = 0;   // sin flipbook por defecto (textura entera)
        emissionRadius = 45.0f;                   // no emitir lejos de la camara (0 = sin limite)
        uvRect[0] = 0.0f; uvRect[1] = 0.0f; uvRect[2] = 1.0f; uvRect[3] = 1.0f;
        color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 1.0f;
        fade = true; active = true; filtered = true; emAcc = 0.0f;
        for (int i = 0; i < 3; i++) { velLocal[i] = 0.0f; velRandom[i] = 0.0f; posRandom[i] = 0.0f; acceleration[i] = 0.0f; }
        drag = 1.0f; growth = 0.0f; brakeGrowth = 1.0f;
        useColorFinal = false; colorFinal[0] = 1.0f; colorFinal[1] = 1.0f; colorFinal[2] = 1.0f; colorFinal[3] = 1.0f;
        alphaDecays = 1.0f; alphaDeath = 0.0f;
        initialRot  = 0.0f; rotRandom = 0.0f; velRotRandom = 0.0f; turnSignRandom = true;
        appear = 0.0f; fadeNear[0] = fadeNear[1] = 0.0f; fadeFar[0] = fadeFar[1] = 0.0f;
        flipFps = 0.0f; flipOnce = false;
        shape = 0; stretch = 0.05f; trailPoints = 4; trailStep = 1.0f / 30.0f; thicknessLine = 1.0f;
        sys.rate = 0.0f;      // la emision continua la lleva Tick (con cono), no el Core
        sys.spinMin = 0.0f; sys.spinMax = 0.0f; // sin giro azaroso: predecible (y testeable)
        sys.swayAmp = 0.0f;
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::particles; }

    void RenderObject() W3D_OVERRIDE;    // NO dibuja: anota el emisor para el pase de translucidas
    bool DrawsSomething() W3D_OVERRIDE;  // con particulas vivas o con su flecha (overlay): si no, ni se apila
    void Tick(float dt, bool canEmit = true); // sincroniza config; emite el rate SOLO si canEmit (visible); la sim (mundo) corre SIEMPRE
    void Emit(int n);            // rafaga de n particulas YA (lua emitir / harness)

    // "r, g, b, a" <-> color[4] (el idioma del panel y del .w3d de texto)
    std::string TextColor() const;
    void SetTextColor(const std::string& s);
};

// ---------------------------------------------------------------------------
//  El PASE de particulas del frame (ver el bloque de ORDEN DE ESCENA de arriba)
// ---------------------------------------------------------------------------
// vacia la lista de anotados (llamar ANTES del traversal de la escena)
void W3dParticlesClearPending();
// dibuja todos los emisores anotados mirando a la vista bindeada y vacia la lista
// (llamar DESPUES de SceneCollection->Render(), con la modelview = matriz de vista)
void W3dParticlesDrawPending();

// corre TODOS los emisores de la escena (una vez por frame; dt en segundos).
// Respeta 'visible' del arbol: un emisor oculto no emite ni simula.
void W3dParticlesTick(float dt);
// true si algun emisor tiene particulas vivas o esta emitiendo: mantiene vivo
// el redibujado del editor (mismo rol que HasAnimationActive para materiales)
bool W3dParticlesAnimating();
// true si algun emisor visible tiene particulas VIVAS (el render a archivo precalienta si no hay ninguna)
bool W3dParticlesAlive();

#endif
