#ifndef PARTICULAS_H
#define PARTICULAS_H
#include "objects/Objects.h"
#include "WhiskUI/draw/icons.h" // el icono compartido del outliner
#include "gfx/w3dParticles.h"   // el sistema de particulas del Core (billboards)
#include <string>

// ============================================================================
//  Objeto Particulas: el EMISOR 3D que envuelve al sistema de particulas del
//  Core (libs/Whisk3DCore/gfx/w3dParticles.h). El emisor vive en su transform:
//  las particulas NACEN en su origen de MUNDO, disparadas en un CONO alrededor
//  de su eje +Y local (rotar el objeto apunta el chorro), y despues viven en
//  espacio de MUNDO (no siguen al emisor: el polvo de una pisada queda atras).
//
//  Contrato del .w3d de texto (mismos nombres en el panel y el JSON v4):
//      Particulas {
//          textura: "texturas/polvo.png"   // PNG con alpha (relativa al .w3d)
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
//          velRotacion: 90     // giro CONTINUO en grados/seg, signo azaroso por particula
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
//  `orden_pasada` solo ordena los grupos ADENTRO de una malla):
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
//  (W3dMaterialEsLuz, Materials.h). Sin eso, una "particula" hecha con quads
//  de malla le abre un agujero a lo que se dibuje despues y mas lejos.
//
//  El bind lua emitir(obj, n) dispara una RAFAGA de n particulas ya (el polvo
//  de las pisadas, un estallido puntual); con cantidad: 0 el emisor emite
//  SOLO por rafagas. El comando del harness es partinfo (config + vivas).
// ============================================================================
class Particulas : public Object {
public:
    // ---- propiedades (texto + v4 + panel; ver el contrato de arriba) ----
    std::string textura;   // ruta del PNG (con alpha). "" = no se dibuja nada.
    float cantidad;        // particulas por segundo (0 = solo rafagas)
    float vida;            // segundos
    float tam;             // lado del billboard (unidades de mundo)
    float vel;             // velocidad inicial (unidades/seg)
    float dispersion;      // apertura TOTAL del cono (grados)
    float gravedad;        // aceleracion -Y de mundo (+ cae, - sube)
    bool  aditivo;         // mezcla aditiva vs alpha
    // MEZCLA SUSTRACTIVA (dst = dst - src): OSCURECE en vez de aclarar. La usa la
    // mitad de las particulas de un emisor tipico (el humo de una chimenea, el
    // polvo del aterrizaje): un humo gris sobre un cielo claro no se puede hacer
    // ni con alpha ni con aditiva. El Core ya la tenia (w3dEngine::MezclaSubtract,
    // REVERSE_SUBTRACT en PC/WebGL y una aproximacion documentada en GL ES 1.1 del
    // N95); lo que faltaba era poder pedirla desde el emisor. Gana sobre `aditivo`.
    bool  sustractivo;     // true = dst - src (humo, polvo)
    float color[4];        // tinte r,g,b + alpha inicial
    bool  desvanecer;      // alpha -> 0 con la vida
    bool  activo;          // false = no emite nada (las vivas terminan su vida)
    // AZAR (feedback del dueno: "el juego original era mas azaroso"). Defaults 0 =
    // comportamiento exacto de antes (round-trip de escenas viejas intacto).
    float variacion;       // 0..1: jitter POR PARTICULA sobre vel, vida y tam (0.3 = +-30%)
    float turbulencia;     // deriva azarosa SUAVE por particula durante la vida (unidades/s^2)
    // ROTACION DEL BILLBOARD, girado en su plano (feedback del dueno: "me parece
    // que a las particulas le falta rotaciones"). El Core ya rota el quad por
    // p.rot/p.spin (DrawBillboard recibe right/up y los gira por particula); lo
    // que faltaba era CONTROLARLO desde el emisor: el Core sorteaba un rot
    // azaroso siempre y el emisor dejaba spin en 0. Ahora el emisor decide:
    bool  rotacion;        // true = nace con angulo azaroso 0..360 FIJO (rotz del original);
                           // false (default) = nace derecha (angulo 0, deterministico)
    float velRotacion;     // grados/seg de giro continuo, SIGNO azaroso por particula (0 = sin giro)

    // ---- runtime (no se guarda) ----
    w3dEngine::ParticleSystem sys; // el sistema del Core: simula y dibuja
    float emAcc;                   // acumulador de la emision continua (cantidad/seg)

    Particulas(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "Particulas", pos) {
        cantidad = 10.0f; vida = 1.0f; tam = 0.3f; vel = 1.5f;
        dispersion = 30.0f; gravedad = 0.0f; aditivo = false; sustractivo = false;
        variacion = 0.0f; turbulencia = 0.0f;
        rotacion = false; velRotacion = 0.0f;   // billboards derechos y quietos (deterministico)
        color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 1.0f;
        desvanecer = true; activo = true; emAcc = 0.0f;
        sys.rate = 0.0f;      // la emision continua la lleva Tick (con cono), no el Core
        sys.spinMin = 0.0f; sys.spinMax = 0.0f; // sin giro azaroso: predecible (y testeable)
        sys.swayAmp = 0.0f;
    }
    ObjectType getType() W3D_OVERRIDE { return ObjectType::particulas; }

    void RenderObject() W3D_OVERRIDE;  // NO dibuja: anota el emisor para el pase de translucidas
    void Tick(float dt, bool puedeEmitir = true); // sincroniza config; emite el rate SOLO si puedeEmitir (visible); la sim (mundo) corre SIEMPRE
    void Emitir(int n);            // rafaga de n particulas YA (lua emitir / harness)

    // "r, g, b, a" <-> color[4] (el idioma del panel y del .w3d de texto)
    std::string ColorTexto() const;
    void SetColorTexto(const std::string& s);
};

// ---------------------------------------------------------------------------
//  El PASE de particulas del frame (ver el bloque de ORDEN DE ESCENA de arriba)
// ---------------------------------------------------------------------------
// vacia la lista de anotados (llamar ANTES del traversal de la escena)
void W3dParticulasLimpiarPendientes();
// dibuja todos los emisores anotados mirando a la vista bindeada y vacia la lista
// (llamar DESPUES de SceneCollection->Render(), con la modelview = matriz de vista)
void W3dParticulasDibujarPendientes();

// corre TODOS los emisores de la escena (una vez por frame; dt en segundos).
// Respeta 'visible' del arbol: un emisor oculto no emite ni simula.
void W3dParticulasTick(float dt);
// true si algun emisor tiene particulas vivas o esta emitiendo: mantiene vivo
// el redibujado del editor (mismo rol que HayAnimacionActiva para materiales)
bool W3dParticulasAnimando();

#endif
