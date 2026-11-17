#ifndef OPCIONESRENDER_H
#define OPCIONESRENDER_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <iostream>
#include <string>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif // o GLES según corresponda

// interpolación
enum { lineal, closest };

// dialecto C++03 compartido
struct RenderType {
    enum Enum { Solid, MaterialPreview, Rendered, ZBuffer, Wireframe, NormalView, Alpha, Vectores };   // Vectores: el pase a PNG y la vista "Motion Vectors" (render/VectoresMovimiento.h)
    Enum v;
    RenderType() : v(Solid) {}
    RenderType(Enum e) : v(e) {}
    operator Enum() const { return v; }
};

// Declaraciones de variables globales
extern RenderType view;

// color de FONDO del RENDER (pase Rendered). GLOBAL y en un solo lugar: es SOLO para el render view (el
// menu Render lo edita). El fondo del viewport en solid/wireframe/material es OTRO color (por-viewport).
extern float g_renderBg[4];
// EL FONDO como TEXTURA 360 (equirectangular): en vez del color, una imagen que rodea la escena, se refleja en lo que
// brilla y alumbra (GLES2 / Vulkan / web y el trazado de rayos). Como un mapa del material: el boton de la derecha de
// la fila "Fondo" cambia entre color y textura. La rotacion (grados, X/Y/Z; Z es arriba) gira la imagen. El color, la
// rotacion y QUE fondo se animan (canales AnimFondo / AnimFondoRot / AnimFondoTex en la coleccion de la escena)
class Texture;
extern int      g_renderBgModo;      // 0 = color, 1 = textura
extern Texture* g_renderBgTex;       // la textura 360 elegida (NULL = ninguna: se ve el color)
extern float    g_renderBgRot[3];    // grados
// las texturas que se usaron de fondo: la pista animable de QUE fondo guarda indice + 1 en esta lista (0 = el color).
// Cada una tiene su referencia (no la libera la purga mientras el proyecto la nombre)
#include <vector>
extern std::vector<Texture*> g_renderBgTexs;
bool W3dFondoConTextura();                       // en modo textura y con una textura cargada
void W3dFondoPonerTextura(Texture* t);           // la elige (la agrega a la lista con su referencia); NULL = ninguna
int  W3dFondoIndiceTextura(Texture* t);          // su lugar + 1 en la lista (0 = no esta / ninguna)
void W3dFondoAplicarCanal(int prop, int comp, float v);   // el gancho de la animacion (g_animFondoHook)
// una DIRECCION de mundo (normalizada) -> el punto de la textura 360, con la rotacion del fondo (u 0..1, v 0..1 de
// arriba a abajo). El cielo de la vista, los reflejos y el trazado de rayos usan esta MISMA cuenta
void W3dFondoDireccionUV(const float d[3], float* u, float* v);
// la MATRIZ de la rotacion del fondo (3x3, filas): mundo -> espacio de la imagen
void W3dFondoMatriz(float m[9]);
void W3dFondoLimpiar();   // al abrir otro proyecto: color negro, sin textura, sin rotacion, lista vacia

// ASPECTO del render (ancho/alto). La GEOMETRIA de las camaras (el rectangulo del "film" + el triangulito de
// arriba) se adapta a esto: 1:1 = cuadrada, 4:3 = 4:3, etc. Se actualiza al cambiar Width/Height del render.
extern float g_renderAspect;

extern GLfloat MaterialPreviewAmbient[4];
extern GLfloat MaterialPreviewDiffuse[4];
extern GLfloat MaterialPreviewSpecular[4];
extern GLfloat MaterialPreviewPosition[4];

// MASTER de overlays: false = "limpieza de pantalla" (sin grilla, gizmos, CONTORNOS de seleccion,
// ni overlay de edit -verts/bordes/caras-). El render del core (Mesh::Render) lo lee para no dibujar
// el contorno/edit. El viewport lo setea = showOverlays por frame antes de renderizar la escena.
extern bool g_mostrarOverlays;
// MODO JUEGO PURO (N95 VERDE+0, LayoutJuegoPuroToggle): el 3D del juego a pantalla
// completa y NADA del editor (menu, headers, overlays, gizmos, passepartout gris).
// TRES estados que cicla la tecla: 0 = editor normal; 1 = juego con BANDAS
// (letterbox del aspecto declarado, lo que ve el jugador del aparato);
// 2 = COVER (el frustum llena el viewport, sin bandas ni culling de borde, y la
// UI del juego se adapta al viewport). Lo leen w3dlayout y ViewPort3D.
extern int g_juegoPuro;
// overlay de normales (toggles + tamano de la linea). Solo en meshes seleccionadas.
extern bool OverlayVertexNormal; // amarillo: promedio de caras por POSICION
extern bool OverlayCustomNormal; // magenta: normal guardada por vertice
extern bool OverlayFaceNormal;   // cian: normal de cada cara (triangulo)
extern float OverlayNormalSize;  // largo de la linea (default 0.10)

// overlay de estadisticas (texto blanco arriba a la derecha del viewport 3D). Submenu "Statistics": cada linea
// se prende/apaga por separado. Los 6 flags (OverlayStatVertices/Faces/Modgen/Times/GL, OverlayFps) son AHORA
// miembros PER-VIEWPORT de Viewport3D (ver ViewPort3D.h), NO globales -> stats independientes por viewport.
extern float g_fpsActual;        // FPS actual (lo actualiza cada plataforma 1x/frame)
extern int   g_renderCaras;      // caras dibujadas por el ultimo pase de escena del viewport activo ([PERF])
extern int   g_renderDraws;      // draw calls idem
extern long g_genMallaCount;     // DIAGNOSTICO: veces que se regenero la malla de un modificador (subsurf/screw).
                                 // Se muestra en Statistics; al ROTAR NO debe subir (la malla se cachea en genValido).
extern bool g_mallasEditadas;    // lo prenden las dos puertas al render de la malla (RefrescarRender/GenerarRender). Un
                                 // BOOLEAN cuyo target se edito tiene que regenerarse; el flag evita recorrer la escena
                                 // por frame: solo se chequea (y se limpia) cuando ALGUNA malla cambio.
extern bool g_objetosMovidos;    // lo prenden los transforms de OBJETO (mover/rotar/escalar/snap). El unico modificador
                                 // que depende de la posicion en el mundo es el MIRROR con TARGET: si el objeto o su
                                 // target se movieron, hay que regenerar su preview. Se chequea/limpia 1x/frame.

// render EVENT-DRIVEN: el loop (PC/Symbian) solo redibuja si g_redraw esta en true
// (lo prende cualquier input / resize) o si hay una animacion en play. Sino no hace
// nada -> CPU casi 0 en reposo (como Blender). Cada plataforma lo prende en su input.
extern bool g_redraw;

// === EDITOR: ajustes de transformacion (NO son del core/motor) ===
// punto de pivote de la transformacion (rotar/escalar): desde donde y como giran
// los objetos y sub-elementos de malla. Estilo Blender. Default = MedianPoint.
enum TransformPivot { PivotCursor3D, PivotIndividual, PivotMedian, PivotActive };
extern int g_transformPivot;
// editar la malla 3D SIN tocar las normales (checkbox). Si esta en false (default),
// las normales se recalculan al CONFIRMAR el move/rotate/scale (no frame a frame).
// distancia del Merge > By Distance (suelda verts a <= esta distancia). Default 0.0001 m (como Blender).
extern float g_mergeDist;
// modificadores: true durante el RENDER final -> Subdivision usa subRenderLevel en vez de subLevel (viewport).
extern bool g_modRenderMode;
// Auto Merge (menu Mesh, opt-in): al confirmar un move suelda los verts movidos con los que queden a <= threshold.
extern bool g_autoMerge;
extern float g_autoMergeThreshold;

// Declaración de función
RenderType StringToRenderType(const std::string& s);

// LAS OPCIONES DEL RENDER A ARCHIVO (tarjeta Render): son del PROGRAMA, no de un panel. F12 / Ctrl+F12 y los botones
// renderizan con esto aunque no haya ningun panel de Propiedades; los paneles solo lo muestran y lo editan
struct W3dRenderOpc {
    std::string carpeta, nombre;      // la salida (carpeta vacia = la de salida por defecto: en Android, Descargas)
    float w, h;                       // el tamano en pixeles (puede ser mayor que la ventana: va por tiles)
    bool zbuffer, normal, alfa;       // los pases extra
    bool vectores, desenfoque;        // el pase de vectores de movimiento y el desenfoque de movimiento del beauty
    float obturador;
    int formato;                      // 0 PNG, 1 JPG, 2 MKV, 3 MP4 (los videos con el ffmpeg de la maquina)
    float calidad;                    // JPG y video, 1..100 (100 = sin perdida en video)
    bool final, finalAlfa, post;      // la COMPOSICION FINAL (se puede no guardar), con alfa (PNG RGBA) y postprocesado
    // EL AUDIO DE LA ESCENA: el video "con sonido" y el boton "Renderizar audio". Formato 0 WAV, 1 FLAC, 2 MP3, 3 AAC,
    // 4 Opus (el WAV lo escribe Whisk3D; los demas, el ffmpeg de la maquina); el bitrate es de los que pierden
    bool sonido;
    int audioFormato;
    float audioKbps;
    W3dRenderOpc();
};
extern W3dRenderOpc g_render;
// sube cuando las opciones cambian de golpe (se abrio un proyecto): los paneles rearman sus filas
extern unsigned g_renderGen;

#endif // OPCIONESRENDER_H