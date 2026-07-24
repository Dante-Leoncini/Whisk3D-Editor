#ifndef COMPILARJUEGO_H
#define COMPILARJUEGO_H

class UI;

// ============================================================================
//  CompilarJuego — el boton "Compilar juego" del editor: convierte el PROYECTO
//  (la carpeta donde vive el script del UI) en un paquete listo para
//  distribuir, informando el progreso paso a paso (notificaciones):
//
//    1. exporta el arbol a build/<nombre>.w3dui
//    2. compila cada .lua a .luac (bytecode, con el luac que compila junto
//       al editor) — los .lua ORIGINALES quedan legibles en el proyecto
//    3. empaqueta todo (w3dui + luac + assets referenciados) en UN archivo
//       build/<nombre>.w3dgame ofuscado (XOR rotante)
//
//  Compilar el ENGINE del juego sigue siendo independiente del editor (cmake
//  de siempre, ver Whisk3D-Examples); esto empaqueta los DATOS del juego.
// ============================================================================
bool CompilarJuego(UI* u);

#endif // COMPILARJUEGO_H
