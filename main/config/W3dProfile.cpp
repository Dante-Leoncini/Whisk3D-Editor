/**
 * @file W3dProfile.cpp
 * @brief Reloj de pared + acumulador de tiempos por categoria del frame.
 *
 * Definiciones de lo declarado en W3dProfile.h. Se compila en TODAS las plataformas
 * (el reloj cambia segun el OS). Lo consume el overlay "Statistics" del Viewport3D.
 */
#include "W3dProfile.h"

#ifdef W3D_SYMBIAN
#include <e32std.h> // User::NTickCount
#else
#include <SDL2/SDL.h> // SDL_GetPerformanceCounter/Frequency (reloj de alta resolucion)
#endif

// Se ACUMULA durante el frame en curso; se resetea al empezar cada frame.
W3dProf g_prof = {0,0,0,0,0};
// Promedio EXPONENCIAL suavizado -> lo que dibuja el overlay (numeros estables).
W3dProf g_profShow = {0,0,0,0,0};

double W3dNowMs() {
#ifndef W3D_SYMBIAN
    return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency(); // reloj de PARED (alta res)
#else
    return (double)User::NTickCount(); // N95: nanokernel tick ~= ms (mismo reloj de pared que LayoutTickFPS)
#endif
}

void W3dProfBegin() { g_prof.logic = g_prof.scene = g_prof.viewport3d = g_prof.render = g_prof.swap = 0.0; }

void W3dProfEnd() {
    g_profShow.logic      = g_profShow.logic      * 0.9 + g_prof.logic      * 0.1;
    g_profShow.scene      = g_profShow.scene      * 0.9 + g_prof.scene      * 0.1;
    g_profShow.viewport3d = g_profShow.viewport3d * 0.9 + g_prof.viewport3d * 0.1;
    g_profShow.render     = g_profShow.render     * 0.9 + g_prof.render     * 0.1;
    g_profShow.swap       = g_profShow.swap       * 0.9 + g_prof.swap       * 0.1;
}
