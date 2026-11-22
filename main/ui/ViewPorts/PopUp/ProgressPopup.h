#ifndef PROGRESSPOPUP_H
#define PROGRESSPOPUP_H

#include <string>
#include <vector>
#include "PopUpBase.h"

class Card;
class Rec2D;

// Barra de progreso modal (4 OS), MISMO estilo que el popup de borrar (Card + borde verde, centrado en
// PC / barra inferior en Symbian). Se usa para operaciones LARGAS y BLOQUEANTES (exportar/importar OBJ en
// el N95): mientras la operacion corre, llama ProgressUpdate(frac) y se redibuja un frame -> el usuario
// ve la barra cargar y no parece que se trabo. Texto en ingles.
class ProgressPopup : public PopUpBase {
    public:
        std::string message; // "Exporting model..." / "Importing model..."
        float frac;          // 0..1
        std::vector<std::string> lines;   // los renglones de abajo (cuadro N de M, tiempos, lo que falta)
        bool withCancel;    // el boton "Cancelar" (un render largo)
        int cancelX, cancelY, cancelW, cancelH;    // donde quedo el boton (en pantalla)
        Card* barBg;         // tarjeta de la barra (fondo + borde), IGUAL que un slider del color picker
        Rec2D* rect;         // relleno verde proporcional (accent @ 0.55), mismo Rec2D que el slider del color

        ProgressPopup();
        ~ProgressPopup();

        int Padding() const;
        int BarHeight() const;
        void Layout();
        void Render();    // popup COMPLETO (fondo + borde + mensaje + barra). Se dibuja UNA vez al iniciar.
        void RenderBar(); // SOLO la barra (track opaco + relleno). Cada update redibuja esto y nada mas.
        void DrawBar();   // el contenido de la barra (sin initView): lo usan Render y RenderBar
        void Draw();      // el popup completo en la posicion x/y ACTUAL (sin Layout, sin clear, sin
                             // swap): lo comparten Render (modal centrado) y el overlay del build
};

extern ProgressPopup* progressPopup;

// ---- API compartida (no-op si no se llamo ProgressStart -> el harness sin GUI no rompe) ----
void ProgressStart(const std::string& msg); // abre la barra (frac 0) y dibuja un primer frame
void ProgressUpdate(float frac);            // setea el avance + redibuja (con throttle); frac en 0..1
void ProgressUpdateFull(float frac);        // como Actualizar pero redibuja el popup ENTERO (clear+fondo+barra):
                                              // para operaciones que PISAN el framebuffer (render por tiles)
void ProgressEnd();                           // cierra la barra
bool ProgressActive();                        // true entre Iniciar y Fin (lo usa la red de seguridad del dock)
// EL RANGO de la barra para lo que sigue: ProgressUpdate(f) muestra a + (b - a) * f (un cuadro de una animacion
// trazada: el trazador cuenta de 0 a 1 su cuadro, la barra muestra el avance de TODA la animacion). (0, 1) = normal
void ProgressRange(float a, float b);
// UNA ANIMACION: el cuadro que empieza ('actual' desde 1) de 'total'. Con eso la barra muestra "Cuadro N de M", lo que
// tardo el ultimo, el promedio y cuanto falta (aproximado)
void ProgressFrame(int current, int total);
// CANCELAR: el boton (y Escape en la PC, la tecla derecha o la C en el telefono). Lo largo pregunta Cancelado() y corta
void ProgressWithCancel(bool yes);
void ProgressCancel();
bool ProgressCanceled();
bool ProgressButtonCancelIn(int x, int y);   // (el click de la PC)
// hook de PLATAFORMA: atender la entrada pendiente mientras algo largo bloquea (PC: los eventos de SDL; Symbian: las
// teclas, con una espera anidada). Se llama desde la barra, a lo sumo cada 1/5 de segundo
extern void (*W3dProgressPoll)();

// ---- overlay NO MODAL (el build de "Compilar juego") ----
// El MISMO widget/estetica que la barra modal del export, pero dibujado DENTRO del
// frame normal del editor (sin clear ni swap propios) y anclado ABAJO: no captura
// input ni bloquea nada. El hilo principal la llama 1x por frame mientras compila.
void ProgressOverlayRender(const std::string& stage, float frac);

// hook de PLATAFORMA: intercambiar buffers (PC = SDL_GL_SwapWindow; Symbian = egl swap). Lo setea cada OS.
extern void (*LayoutSwapBuffers)();

#endif
