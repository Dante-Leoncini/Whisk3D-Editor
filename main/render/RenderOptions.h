#ifndef RENDER_OPTIONS_H
#define RENDER_OPTIONS_H

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
enum { linear, closest };

// dialecto C++03 compartido
struct RenderType {
    enum Enum { Solid, MaterialPreview, Rendered, ZBuffer, Wireframe, NormalView, Alpha, Vectors };   // Vectores: el pase a PNG y la vista "Motion Vectors" (render/MotionVectors.h)
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
// rotacion y QUE fondo se animan (canales AnimBackground / AnimBackgroundRot / AnimBackgroundTex en la coleccion de la escena)
class Texture;
extern int      g_renderBgMode;      // 0 = color, 1 = textura
extern Texture* g_renderBgTex;       // la textura 360 elegida (NULL = ninguna: se ve el color)
extern float    g_renderBgRot[3];    // grados
// LA LUZ DEL FONDO: cuanto ilumina el fondo (0 = nada, 1 = 100%, hasta 2 = 200%). El fondo es la luz AMBIENTE de la vista
// Render y del juego: con un color, ese color; con una textura 360, su irradiancia (SceneRender.cpp). Tambien el
// trazado de rayos la usa para la luz del cielo. Lo que se VE del fondo no cambia
extern float    g_renderLightBackground;
// las texturas que se usaron de fondo: la pista animable de QUE fondo guarda indice + 1 en esta lista (0 = el color).
// Cada una tiene su referencia (no la libera la purga mientras el proyecto la nombre)
#include <vector>
extern std::vector<Texture*> g_renderBgTexs;
bool W3dBackgroundWithTexture();                       // en modo textura y con una textura cargada
// el color del "Clear color" en AUTO (objects/Routine.h): el fondo del render; con un fondo 360 que se ve (el pase con las
// luces: Render y el juego) ninguno (false: lo tapa el cielo). El visor 3D del editor, fuera de la vista Render, pone el
// suyo mientras dibuja (W3dBackgroundAutoSet(su color) ... W3dBackgroundAutoSet(NULL))
bool W3dBackgroundAutoColor(float c[4]);
void W3dBackgroundAutoSet(const float* c);
void W3dBackgroundSetTexture(Texture* t);            // la elige (la agrega a la lista con su referencia); NULL = ninguna
int  W3dBackgroundTextureIndex(Texture* t);          // su lugar + 1 en la lista (0 = no esta / ninguna)
void W3dBackgroundApplyChannel(int prop, int comp, float v);   // el gancho de la animacion (g_animBackgroundHook)
// una DIRECCION de mundo (normalizada) -> el punto de la textura 360, con la rotacion del fondo (u 0..1, v 0..1 de
// arriba a abajo). El cielo de la vista, los reflejos y el trazado de rayos usan esta MISMA cuenta
void W3dBackgroundDirectionUV(const float d[3], float* u, float* v);
// la MATRIZ de la rotacion del fondo (3x3, filas): mundo -> espacio de la imagen
void W3dBackgroundMatrix(float m[9]);
void W3dBackgroundClear();   // al abrir otro proyecto: color negro, sin textura, sin rotacion, lista vacia

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
extern bool g_showOverlays;
// MODO JUEGO PURO (N95 VERDE+0, LayoutPureGameToggle): el 3D del juego a pantalla
// completa y NADA del editor (menu, headers, overlays, gizmos, passepartout gris).
// TRES estados que cicla la tecla: 0 = editor normal; 1 = juego con BANDAS
// (letterbox del aspecto declarado, lo que ve el jugador del aparato);
// 2 = COVER (el frustum llena el viewport, sin bandas ni culling de borde, y la
// UI del juego se adapta al viewport). Lo leen w3dlayout y ViewPort3D.
extern int g_pureGame;
// overlay de normales (toggles + tamano de la linea). Solo en meshes seleccionadas.
extern bool OverlayVertexNormal; // amarillo: promedio de caras por POSICION
extern bool OverlayCustomNormal; // magenta: normal guardada por vertice
extern bool OverlayFaceNormal;   // cian: normal de cada cara (triangulo)
extern float OverlayNormalSize;  // largo de la linea (default 0.10)

// overlay de estadisticas (texto blanco arriba a la derecha del viewport 3D). Submenu "Statistics": cada linea
// se prende/apaga por separado. Los 6 flags (OverlayStatVertices/Faces/Modgen/Times/GL, OverlayFps) son AHORA
// miembros PER-VIEWPORT de Viewport3D (ver ViewPort3D.h), NO globales -> stats independientes por viewport.
extern float g_currentFps;      // FPS actual (lo actualiza cada plataforma 1x/frame)
extern int   g_renderFaces;     // caras dibujadas por el ultimo pase de escena del viewport activo ([PERF])
extern int   g_renderDraws;     // draw calls idem
extern long g_genMeshCount;     // DIAGNOSTICO: veces que se regenero la malla de un modificador (subsurf/screw).
                                 // Se muestra en Statistics; al ROTAR NO debe subir (la malla se cachea en genValid).
extern bool g_meshesEdited;    // lo prenden las dos puertas al render de la malla (RefreshRender/GenerateRender). Un
                                 // BOOLEAN cuyo target se edito tiene que regenerarse; el flag evita recorrer la escena
                                 // por frame: solo se chequea (y se limpia) cuando ALGUNA malla cambio.
extern bool g_objectsMoved;    // lo prenden los transforms de OBJETO (mover/rotar/escalar/snap). El unico modificador
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
struct W3dRenderOpt {
    std::string folder, name;         // la salida (carpeta vacia = la de salida por defecto: en Android, Descargas)
    float w, h;                       // el tamano en pixeles (puede ser mayor que la ventana: va por tiles)
    bool zbuffer, normal, alpha;      // los pases extra
    bool vectors, blur;               // el pase de vectores de movimiento y el desenfoque de movimiento del beauty
    float shutter;
    int format;                        // 0 PNG, 1 JPG, 2 MKV, 3 MP4 (los videos con el ffmpeg de la maquina)
    float quality;                     // JPG y video, 1..100 (100 = sin perdida en video)
    bool final, finalAlpha, post;      // la COMPOSICION FINAL (se puede no guardar), con alfa (PNG RGBA) y postprocesado
    // EL AUDIO DE LA ESCENA: el video "con sonido" y el boton "Renderizar audio". Formato 0 WAV, 1 FLAC, 2 MP3, 3 AAC,
    // 4 Opus (el WAV lo escribe Whisk3D; los demas, el ffmpeg de la maquina); el bitrate es de los que pierden
    bool sound;
    int audioFormat;
    float audioKbps;
    // EL VIDEO (MKV / MP4): el codec (0 H.264, 1 MPEG-4 parte 2; si la maquina no tiene el elegido, el otro) y su bitrate
    int videoCodec;
    float videoKbps;
    // ANTIALIASING del render rasterizado (sin trazado de rayos): cada pixel sale de antialias x antialias muestras
    // (supermuestreo: se dibuja mas grande y se promedia). 1 = apagado
    int antialias;
    W3dRenderOpt();
};
extern W3dRenderOpt g_render;
// sube cuando las opciones cambian de golpe (se abrio un proyecto): los paneles rearman sus filas
extern unsigned g_renderGen;

// DIBUJAR FUERA DE LA PANTALLA: lo que se dibuja para LEERLO y nunca se muestra (las franjas del render a archivo). NULL =
// en la ventana (la PC, Android: nada se ve hasta el swap). En el N95 lo dibujado en la ventana SE VEIA antes del swap -las
// franjas del render en la barra de progreso-: ahi va a una superficie aparte (pbuffer). Se puede anidar.
// Empezar devuelve false si no hay superficie aparte (entonces se dibuja en la ventana, como siempre)
extern bool (*W3dOutsideScreenBegin)(int w, int h);
extern void (*W3dOutsideScreenFinish)();
#endif // RENDER_OPTIONS_H