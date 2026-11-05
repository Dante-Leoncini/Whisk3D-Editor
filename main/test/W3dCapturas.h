#ifndef W3D_CAPTURAS_H
#define W3D_CAPTURAS_H
#include <string>
#include <sstream>

// ============================================================================
//  W3dCapturas — comandos de harness para las CAPTURAS de los tutoriales y del
//  manual (tools/capturas/). Manejan el editor por la MISMA puerta que una
//  persona: cada evento entra por InputUsuarioSDL3, como los de SDL.
//
//    raton down|move|up <x> <y> [izq|medio|der]   el mouse (boton izquierdo por defecto)
//    dedo  down|move|up <x> <y> [id]              un dedo en la pantalla tactil: el mouse
//                                                 que sintetiza SDL + el evento de dedo
//    rueda <pasos> <x> <y>                        la rueda del mouse sobre (x, y)
//    tecla <nombre> [tap|down|up]                 una tecla por nombre de SDL ("m", "Right",
//                                                 "Return", "Escape"...); tap = apretar y soltar
//    escala <1..6>                                la escala de la interfaz (1 = como en el N95)
//    cuadro                                       dibuja un cuadro (cada evento ya dibuja uno despues,
//                                                 como el loop del editor)
//    capinfo                                      imprime lineas "@cap ..." con la geometria
//                                                 de cada viewport, su boton de la esquina y
//                                                 las filas del menu abierto (para apuntar)
//
//  Coordenadas en pixeles del layout (las de 'ventana <w> <h>' y de 'uishot').
//  Ver tools/capturas/README.md.
// ============================================================================
bool W3dCapturasCmd(const std::string& cmd, std::istringstream& ss,
                    std::string& err, bool& manejado);

#endif
