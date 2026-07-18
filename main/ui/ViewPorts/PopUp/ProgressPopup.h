#ifndef PROGRESSPOPUP_H
#define PROGRESSPOPUP_H

#include <string>
#include "PopUpBase.h"

class Card;
class Rec2D;

// Barra de progreso modal (4 OS), MISMO estilo que el popup de borrar (Card + borde verde, centrado en
// PC / barra inferior en Symbian). Se usa para operaciones LARGAS y BLOQUEANTES (exportar/importar OBJ en
// el N95): mientras la operacion corre, llama ProgresoActualizar(frac) y se redibuja un frame -> el usuario
// ve la barra cargar y no parece que se trabo. Texto en ingles.
class ProgressPopup : public PopUpBase {
    public:
        std::string mensaje; // "Exporting model..." / "Importing model..."
        float frac;          // 0..1
        Card* barBg;         // tarjeta de la barra (fondo + borde), IGUAL que un slider del color picker
        Rec2D* rect;         // relleno verde proporcional (accent @ 0.55), mismo Rec2D que el slider del color

        ProgressPopup();
        ~ProgressPopup();

        int Padding() const;
        int AltoBarra() const;
        void Layout();
        void Render();       // popup COMPLETO (fondo + borde + mensaje + barra). Se dibuja UNA vez al iniciar.
        void RenderBar();    // SOLO la barra (track opaco + relleno). Cada update redibuja esto y nada mas.
        void DibujarBarra(); // el contenido de la barra (sin initView): lo usan Render y RenderBar
};

extern ProgressPopup* progressPopup;

// ---- API compartida (no-op si no se llamo ProgresoIniciar -> el harness sin GUI no rompe) ----
void ProgresoIniciar(const std::string& msg); // abre la barra (frac 0) y dibuja un primer frame
void ProgresoActualizar(float frac);          // setea el avance + redibuja (con throttle); frac en 0..1
void ProgresoActualizarFull(float frac);      // como Actualizar pero redibuja el popup ENTERO (clear+fondo+barra):
                                              // para operaciones que PISAN el framebuffer (render por tiles)
void ProgresoFin();                           // cierra la barra

// hook de PLATAFORMA: intercambiar buffers (PC = SDL_GL_SwapWindow; Symbian = egl swap). Lo setea cada OS.
extern void (*LayoutSwapBuffers)();

#endif
