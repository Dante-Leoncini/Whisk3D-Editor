#ifndef SCENE_RENDER_H
#define SCENE_RENDER_H

#include "objects/CameraBase.h"
#include <string>

// ============================================================================
//  EscenaRender — EL PASE 3D DE LA ESCENA, UNA SOLA VEZ, PARA LOS DOS.
//
//  El viewport del editor (Viewport3D::Render) y el RUNTIME de un juego
//  compilado (w3drun) tienen que dibujar la escena EXACTAMENTE igual: mismo
//  encuadre, misma proyeccion, mismo orden de pasadas. Hasta que esto existio,
//  el unico que sabia dibujar una escena 3D era el viewport del editor, y el
//  juego compilado salia con la pantalla en negro (solo el HUD 2D). Copiar el
//  bloque del viewport al runtime hubiera dado DOS caminos que se desincronizan
//  al primer cambio, asi que el pase vive ACA y los dos lo llaman.
//
//  Lo que ESTE archivo hace (todo lo que un JUEGO necesita):
//     encuadre declarado (letterbox) -> proyeccion -> camara (bind al Core +
//     matriz de vista) -> recorrido de la escena (que adentro resuelve culling,
//     LOD, visibilidad por celdas, espejos, instancias, luces y lotes) ->
//     calcomanias diferidas -> luces aditivas diferidas -> particulas.
//
//  Lo que NO hace (y por eso el juego no arrastra el editor): grilla, gizmos,
//  contornos de seleccion, huesos, motion trail, iconos, estadisticas y demas
//  chrome. Eso sigue en Viewport3D, DESPUES de llamar aca.
//
//  C++03 a proposito (igual que el resto del Core): esto compila tambien en los
//  targets viejos.
// ============================================================================

// ---------------------------------------------------------------------------
//  ENCUADRE DECLARADO (`aspecto:` de la camara o del riel) -> MARCO.
//
//  Devuelve las MEDIAS-EXTENSIONES en NDC (1 = todo el viewport) del rectangulo
//  con aspecto 'declaredAspect' encajado dentro de un viewport de aspecto
//  'viewAspect'. 'margen' es cuanto del viewport puede ocupar el marco: 1 =
//  todo (letterbox real, lo que ve el jugador), <1 deja aire alrededor (el
//  editor usa 0.92 para que se vea el marco flotando).
//
//  Era la MISMA cuenta escrita en cuatro lugares (proyeccion del viewport,
//  bandas del passepartout, colocacion del HUD y frustum del Culling): ahora
//  hay una sola y el runtime usa esa.
// ---------------------------------------------------------------------------
void W3dFramingFrame(float viewAspect, float declaredAspect, float margin,
                      float* nx, float* ny);

// ---------------------------------------------------------------------------
//  LA VISTA de un pase 3D: todo lo que hace falta para proyectar y mirar.
// ---------------------------------------------------------------------------
struct W3dView3D {
    CameraBase cam;            // posicion + orientacion de la vista
    float fov;                 // campo de vision vertical en grados (perspectiva)
    float nearC, farC;         // planos de recorte
    bool  ortho;               // proyeccion ortografica
    float orthoSize;           // media-altura del volumen en ortografica
    float viewAspect;          // ancho/alto del viewport (px)
    float imageAspect;         // aspecto del frustum que se dibuja (el declarado, o el de la vista)
    float frameNX, frameNY;    // marco del encuadre en NDC (1,1 = sin encuadre)
    float zoom, panX, panY;    // inspeccion del editor (el runtime usa 1,0,0)
    float imageHeightPx;       // alto en px de la imagen (el encuadre adentro del viewport); 0 = no se sabe

    W3dView3D()
        : fov(45.0f), nearC(0.1f), farC(1000.0f), ortho(false), orthoSize(1.0f),
          viewAspect(1.0f), imageAspect(1.0f), frameNX(1.0f), frameNY(1.0f),
          zoom(1.0f), panX(0.0f), panY(0.0f), imageHeightPx(0.0f) {}
};

// Carga la matriz de PROYECCION de esta vista (deja el MatrixMode en ModelView
// con identidad) y PUBLICA la lente al Core (g_renderCam*), que es de donde el
// objeto Culling arma sus 6 planos en CPU. El marco del encuadre entra en el
// frustum: lo de afuera del encuadre no se dibuja aunque haya viewport.
void W3dScene3DProjection(const W3dView3D& v);

// Publica la vista al Core (W3dViewBind) y carga la matriz de VISTA en GL.
// La luz del normal mapping queda en 'lightPos'/'lightColor' (headlight = pos de la
// camara y blanco).
void W3dScene3DCamera(const W3dView3D& v, const Vector3& lightPos, const Vector3& lightColor);

// ---------------------------------------------------------------------------
//  MODO DE DIBUJO: deriva los flags del Core (w3dRenderWireframe/Solido/Luces/...)
//  y la LUZ del pase a partir del RenderType. El editor lo llama con el modo del
//  viewport; el runtime del juego, con el modo del proyecto (MaterialPreview por
//  defecto, que es lo que muestra el Play). Es la misma derivacion para los dos:
//  si el juego eligiera otra, el .deb se veria distinto del Play.
//  'vista' es un RenderType (render/RenderOptions.h) pasado como int para no
//  arrastrar el header del editor a los que solo dibujan.
void W3dScene3DMode(int view);

// LUCES del pase (apagar las 8 de GL + la luz frontal fija de los modos de preview).
// Va SEPARADA de W3dScene3DMode porque la POSICION de una luz de GL se transforma por
// la modelview del momento: los parametros se pueden poner cuando sea, la posicion solo
// con la modelview en identidad. El editor la llama desde ReloadLights (al principio del
// frame) y el runtime del juego antes del pase.
void W3dScene3DLights(int view);

// EL PASE: recorrido de la escena + los tres diferidos, en el orden que el
// editor viene usando (calcomanias -> luces aditivas -> particulas). No limpia
// la pantalla ni toca la proyeccion: eso lo decide el que llama.
void W3dScene3DPass();
// EL FONDO de una imagen del render a archivo (las franjas de ViewPort3D::RenderStripes, que dibujan la escena por su
// cuenta): el entorno de los reflejos y el CIELO 360, como el pase de la vista. Fin: lo apaga (despues de la escena)
void W3dScene3DBackgroundFile();
void W3dScene3DBackgroundFileEnd();

// BANDAS del encuadre (letterbox/pillarbox) sobre un rect de 'W'x'H' pixeles.
// 'opaco' = negro pleno (lo que ve el jugador en el aparato: el RUNTIME siempre
// las dibuja asi); 'opaco' falso = negro al 50% (el editor, para poder auditar
// lo que queda afuera del encuadre). Dibuja en 2D; el que llama restaura lo suyo.
// el marco cubre todo el rect: no hay bandas (el que dibuja se ahorra el 2D entero)
bool W3dScene3DNoBands(float W, float H, float frameNX, float frameNY, float zoom, float panX, float panY);
void W3dScene3DBands(float W, float H, float frameNX, float frameNY,
                       float zoom, float panX, float panY, bool opaque);

// ---------------------------------------------------------------------------
//  CAPTURAS DE PANTALLA: la imagen 3D de un cuadro copiada a una textura (render a textura). Sirve para fundir la
//  ultima imagen sobre la nueva en un corte, para un espejo o una pantalla dentro del juego (una rutina dibuja algo,
//  lo copia y despues dibuja lo demas con esa textura), para congelar la imagen detras de un menu.
//
//  Cada captura tiene un NOMBRE y su textura se llama "pantalla:<nombre>": con esa ruta la usan un material, una
//  imagen 2D (setTextura) o el paso "Bind texture" de una rutina. Se copia el CUADRO de la imagen (el encuadre de la
//  camara, sin las bandas) en pixeles reales: la textura es la potencia de dos que lo contiene (crece sola si la
//  pantalla cambia) y la imagen queda en su esquina, DADA VUELTA en V (la fila 0 es la de abajo): W3dCaptureUV da el
//  rectangulo de UV que la muestra derecha. Una captura cuesta una copia en la GPU; en el PowerVR del N95, ademas, el
//  dibujo de lo mandado hasta ese momento: es para un cuadro de vez en cuando.
// ---------------------------------------------------------------------------
// la textura de una captura ("nombre" o "pantalla:nombre"); 'crear' = la reserva si no existe (con el tamano del
// viewport actual). NULL si no existe y no se pidio crearla
class Texture;
Texture* W3dCaptureTexture(const std::string& name, bool create);
bool     W3dIsCapturePath(const std::string& path);   // empieza con "pantalla:"
// lua (capturarPantalla): la copia se hace al terminar el pase 3D de ESTE cuadro (antes del HUD) o, 'withHud', despues
// de dibujar el HUD (la pantalla entera, como la ve el jugador)
void     W3dCaptureRequest(const std::string& name, bool withHud = false);
// el paso "Copy screen" de una rutina: copia YA el cuadro de la imagen (lo dibujado hasta ahora)
bool     W3dCaptureCopyNow(const std::string& name);
// u0, v0, u1, v1 de la parte de la textura con la imagen, derecha (v0 > v1). false si nunca se copio o si hay una copia
// pedida que todavia no se hizo (el cuadro no se dibujo: el que la pidio espera)
bool     W3dCaptureUV(const std::string& name, float uv[4]);
// las pedidas (lo llama W3dScene3DPass al terminar) y las pedidas con el HUD (lo llama quien dibuja el HUD del
// juego, despues de dibujarlo: el viewport del editor y el runtime)
void     W3dCapturesDo();
void     W3dCapturesDoWithHud();

#endif // SCENE_RENDER_H
