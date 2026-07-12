#ifndef UVEDITOR_H
#define UVEDITOR_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h" // borde (verde si activo) como Outliner/Properties

class Mesh; // pick UV (la seleccion vive en Mesh::uvSelVert)

// =====================================================================
//  UV Editor (2D) — por ahora un VISOR (no edita todavia).
// =====================================================================
//  - Si el objeto activo NO es una malla: no muestra nada (solo el fondo).
//  - Si hay una malla: muestra la TEXTURA de la mesh part activa/seleccionada,
//    centrada, con el WIREFRAME de las UV encima.
//  - Rueda del mouse = zoom; boton del MEDIO + arrastrar = paneo.
//  - Menu del viewport (boton de icono): checkbox "Show Whole Mesh UV" (todo el
//    mesh vs solo la parte activa) y "Repeat Texture" (tiling hasta el infinito).
class UVEditor : public ViewportBase, public WithBorder {
    public:
        int ViewportKind() const { return 4; } // 1=3D 2=outliner 3=properties 4=UV

        float zoom;          // 1 = la textura llena ~80% del viewport
        float panX, panY;    // desplazamiento en pixeles (paneo)
        // Sync Selection: ON = ves TODO el UV del modelo, las caras seleccionadas en 3D resaltadas
        // (verde) y el resto en gris. OFF = solo ves las caras SELECCIONADAS, lo demas no se dibuja.
        bool  syncSelection;
        bool  repeatTexture; // true: textura repetida (tiling)
        bool  mostrarChromeUV; // overlay LIVE de las UV del reflejo chrome equirect (demo/debug; off = 0 CPU extra)
        int   lastMx, lastMy;// para el delta del paneo

        // --- TRANSFORM 2D de la seleccion UV (G/R/S en PC; 1/2/3 en Symbian) ---
        int   uvXform;              // 0=nada 1=mover 2=rotar 3=escalar
        float uvXPivotU, uvXPivotV; // pivot (por ahora el centroide de la seleccion)
        float uvXStartU, uvXStartV; // posicion del mouse al iniciar, en UV
        std::vector<int>   uvXIdx;  // verts de render seleccionados (los que se mueven)
        std::vector<float> uvXOrig; // su uv ORIGINAL (2 por vert): base estable + para cancelar
        float uvCursorU, uvCursorV; // CURSOR 2D (en UV): pivot opcional (modo "3D Cursor") + snap
        int   uvSelMode;            // modo de seleccion PROPIO del UV (SelVertex/Edge/Face), independiente
                                    // del 3D. En sync (syncSelection) se usa el del 3D. El efectivo = ModoUV().

        UVEditor();
        virtual ~UVEditor();

        void Render() override;
        void Resize(int newW, int newH) override;
        void event_mouse_motion(int mx, int my) override;
        bool event_finger_scroll(int px, int py, int dx, int dy) override;             // 1 dedo: panear la vista UV
        void event_finger_gesture(float zoomDelta, float panDx, float panDy) override; // 2 dedos: zoom + paneo
        void button_left() override; // click izquierdo = seleccionar UV (o confirmar transform)
        void button_right() override; // click derecho = cancelar transform
        // pick en coords LOCALES del viewport (lx,ly); add=true -> toggle/sumar (shift), false -> reemplazar.
        // Escribe Mesh::uvSelVert. Compartido (PC lo llama desde button_left; Symbian desde OK).
        void PickUV(Mesh* m, int lx, int ly, bool add);
        // modo de seleccion EFECTIVO: si syncSelection -> EditSelectMode (3D); si no -> uvSelMode (propio).
        int ModoUV() const;
        // parametros del mapeo UV->pantalla (centro cx,cy y escala s), iguales que el Render.
        void ParamsUV(float& cx, float& cy, float& s) const;
        // transform 2D (COMPARTIDO; Symbian llama Iniciar desde 1/2/3 y Confirmar/Cancelar desde sus teclas).
        void IniciarXform(Mesh* m, int modo);          // 1=mover 2=rotar 3=escalar (snapshot + pivot)
        void AplicarXform(Mesh* m, float curU, float curV); // aplica segun el modo, en vivo
        void ConfirmarXform();                          // fija el cambio
        void CancelarXform(Mesh* m);                    // restaura los uv originales
        // SNAP (menu del UV editor): cursor<->seleccion.
        void SnapCursorToSel();  // el cursor 2D va al centro de la seleccion
        void SnapSelToCursor();  // la seleccion se mueve para que su centro quede en el cursor
        void CursorToCenter();   // cursor 2D -> (0.5,0.5)
        void Panear(float dx, float dy); // paneo de la vista UV (flechas; COMPARTIDO PC/Symbian -> FUERA del #ifndef SDL)
        void ZoomCentro(int dir);        // zoom centrado en el viewport (sin cursor): teclado 0+arriba/abajo (Symbian)
#ifndef W3D_SYMBIAN
        void event_key_down(SDL_Event &e) override; // G/R/S + ESC/ENTER (transform)
        void event_mouse_wheel(SDL_Event &e) override;
        // IMPRESCINDIBLE: resetear ViewPortClickDown al soltar (sino queda en true y
        // viewPortActive se CONGELA -> el borde verde no cambia + no se puede resize).
        void mouse_button_up(SDL_Event &e) override;
#endif
};

// dropdown "Texture" de la barra del UV editor: fija a mano que parte/textura del modelo se muestra (-1 = auto).
// Lo llama LayoutClickBarraUV (LayoutInput.cpp) al elegir una opcion del menu.
void UVSetTexOverride(Mesh* m, int part);

#endif
