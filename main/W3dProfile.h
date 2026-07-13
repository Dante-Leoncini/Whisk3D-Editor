#ifndef W3DPROFILE_H
#define W3DPROFILE_H

// Profiler simple del frame: mide en milisegundos cuanto tarda cada parte, para saber a QUE atacar cuando los fps
// estan bajos (ej: LISA, low-poly, deberia volar). Se muestra en el overlay "Statistics" del viewport 3D.
//   logic     = input (eventos SDL) + tick de animacion + notificaciones + carga diferida
//   scene     = SceneCollection->Render (dibujar la escena: skinning + geometria de los modelos)
//   viewport3d= todo el Viewport3D::Render (scene + grilla + overlays + huesos + la UI del viewport)
//   render    = TODO rootViewport->Render (los N viewports 3D + los paneles de UI outliner/props/timeline + menus)
//   swap      = SDL_GL_SwapWindow (con vsync ON = la espera al vblank; si es alto, hay HOLGURA de CPU/GPU)
// Derivados utiles para leer: UI-paneles = render - viewport3d ; overhead-3D = viewport3d - scene.
// PC/desktop/web usan el reloj de alta resolucion de SDL; en Symbian W3dNowMs devuelve 0 (queda en 0, inofensivo).
struct W3dProf { double logic, scene, viewport3d, render, swap; };
extern W3dProf g_prof;      // se ACUMULA durante el frame en curso (se resetea al empezar cada frame)
extern W3dProf g_profShow;  // promedio EXPONENCIAL suavizado -> lo que dibuja el overlay (numeros estables)
double W3dNowMs();          // reloj de pared en milisegundos (alta resolucion)
void W3dProfBegin();        // arranca el frame: resetea g_prof (lo llaman los DOS main loops: PC main.cpp y Symbian w3dlayout.cpp)
void W3dProfEnd();          // cierra el frame: suaviza g_profShow hacia g_prof

#endif // W3DPROFILE_H
