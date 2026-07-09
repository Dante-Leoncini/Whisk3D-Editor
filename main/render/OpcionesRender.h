#ifndef OPCIONESRENDER_H
#define OPCIONESRENDER_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include <iostream>
#ifdef W3D_SYMBIAN
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif // o GLES según corresponda

// interpolación
enum { lineal, closest };

// dialecto C++03 compartido
struct RenderType {
    enum Enum { Solid, MaterialPreview, Rendered, ZBuffer, Wireframe, NormalView, Alpha };
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
// overlay de normales (toggles + tamano de la linea). Solo en meshes seleccionadas.
extern bool OverlayVertexNormal; // amarillo: promedio de caras por POSICION
extern bool OverlayCustomNormal; // magenta: normal guardada por vertice
extern bool OverlayFaceNormal;   // cian: normal de cada cara (triangulo)
extern float OverlayNormalSize;  // largo de la linea (default 0.10)

// overlay de estadisticas (texto blanco arriba a la derecha del viewport 3D)
extern bool OverlayStatistics;   // "vertex: agrupados/reales" + "faces: logicas/triangulos"
extern bool OverlayFps;          // "fps: N"
extern float g_fpsActual;        // FPS actual (lo actualiza cada plataforma 1x/frame)
extern long g_genMallaCount;     // DIAGNOSTICO: veces que se regenero la malla de un modificador (subsurf/screw).
                                 // Se muestra en Statistics; al ROTAR NO debe subir (la malla se cachea en genValido).
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
extern bool g_editLockNormales;
// distancia del Merge > By Distance (suelda verts a <= esta distancia). Default 0.0001 m (como Blender).
extern float g_mergeDist;
// modificadores: true durante el RENDER final -> Subdivision usa subRenderLevel en vez de subLevel (viewport).
extern bool g_modRenderMode;
// Auto Merge (menu Mesh, opt-in): al confirmar un move suelda los verts movidos con los que queden a <= threshold.
extern bool g_autoMerge;
extern float g_autoMergeThreshold;

// Declaración de función
RenderType StringToRenderType(const std::string& s);

#endif // OPCIONESRENDER_H