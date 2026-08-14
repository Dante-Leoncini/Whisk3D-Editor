#ifndef ESCENA_RENDER_H
#define ESCENA_RENDER_H

#include "objects/CameraBase.h"

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
//  con aspecto 'aspectoDeclarado' encajado dentro de un viewport de aspecto
//  'aspectoVista'. 'margen' es cuanto del viewport puede ocupar el marco: 1 =
//  todo (letterbox real, lo que ve el jugador), <1 deja aire alrededor (el
//  editor usa 0.92 para que se vea el marco flotando).
//
//  Era la MISMA cuenta escrita en cuatro lugares (proyeccion del viewport,
//  bandas del passepartout, colocacion del HUD y frustum del Culling): ahora
//  hay una sola y el runtime usa esa.
// ---------------------------------------------------------------------------
void W3dEncuadreMarco(float aspectoVista, float aspectoDeclarado, float margen,
                      float* nx, float* ny);

// ---------------------------------------------------------------------------
//  LA VISTA de un pase 3D: todo lo que hace falta para proyectar y mirar.
// ---------------------------------------------------------------------------
struct W3dVista3D {
    CameraBase cam;            // posicion + orientacion de la vista
    float fov;                 // campo de vision vertical en grados (perspectiva)
    float nearC, farC;         // planos de recorte
    bool  orto;                // proyeccion ortografica
    float ortoSize;            // media-altura del volumen en ortografica
    float aspectoVista;        // ancho/alto del viewport (px)
    float aspectoImagen;       // aspecto del frustum que se dibuja (el declarado, o el de la vista)
    float marcoNX, marcoNY;    // marco del encuadre en NDC (1,1 = sin encuadre)
    float zoom, panX, panY;    // inspeccion del editor (el runtime usa 1,0,0)

    W3dVista3D()
        : fov(45.0f), nearC(0.1f), farC(1000.0f), orto(false), ortoSize(1.0f),
          aspectoVista(1.0f), aspectoImagen(1.0f), marcoNX(1.0f), marcoNY(1.0f),
          zoom(1.0f), panX(0.0f), panY(0.0f) {}
};

// Carga la matriz de PROYECCION de esta vista (deja el MatrixMode en ModelView
// con identidad) y PUBLICA la lente al Core (g_renderCam*), que es de donde el
// objeto Culling arma sus 6 planos en CPU. El marco del encuadre entra en el
// frustum: lo de afuera del encuadre no se dibuja aunque haya viewport.
void W3dEscena3DProyeccion(const W3dVista3D& v);

// Publica la vista al Core (W3dVistaBind) y carga la matriz de VISTA en GL.
// La luz del normal mapping queda en 'luzPos'/'luzColor' (headlight = pos de la
// camara y blanco).
void W3dEscena3DCamara(const W3dVista3D& v, const Vector3& luzPos, const Vector3& luzColor);

// ---------------------------------------------------------------------------
//  MODO DE DIBUJO: deriva los flags del Core (w3dRenderWireframe/Solido/Luces/...)
//  y la LUZ del pase a partir del RenderType. El editor lo llama con el modo del
//  viewport; el runtime del juego, con el modo del proyecto (MaterialPreview por
//  defecto, que es lo que muestra el Play). Es la misma derivacion para los dos:
//  si el juego eligiera otra, el .deb se veria distinto del Play.
//  'vista' es un RenderType (render/OpcionesRender.h) pasado como int para no
//  arrastrar el header del editor a los que solo dibujan.
void W3dEscena3DModo(int vista);

// LUCES del pase (apagar las 8 de GL + la luz frontal fija de los modos de preview).
// Va SEPARADA de W3dEscena3DModo porque la POSICION de una luz de GL se transforma por
// la modelview del momento: los parametros se pueden poner cuando sea, la posicion solo
// con la modelview en identidad. El editor la llama desde ReloadLights (al principio del
// frame) y el runtime del juego antes del pase.
void W3dEscena3DLuces(int vista);

// EL PASE: recorrido de la escena + los tres diferidos, en el orden que el
// editor viene usando (calcomanias -> luces aditivas -> particulas). No limpia
// la pantalla ni toca la proyeccion: eso lo decide el que llama.
void W3dEscena3DPasada();

// BANDAS del encuadre (letterbox/pillarbox) sobre un rect de 'W'x'H' pixeles.
// 'opaco' = negro pleno (lo que ve el jugador en el aparato: el RUNTIME siempre
// las dibuja asi); 'opaco' falso = negro al 50% (el editor, para poder auditar
// lo que queda afuera del encuadre). Dibuja en 2D; el que llama restaura lo suyo.
void W3dEscena3DBandas(float W, float H, float marcoNX, float marcoNY,
                       float zoom, float panX, float panY, bool opaco);

#endif // ESCENA_RENDER_H
