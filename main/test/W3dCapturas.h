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
//                                                 que sintetiza SDL + el evento de dedo. Con
//                                                 id 2 (o mas), un dedo mas: solo el evento de dedo
//                                                 (dos dedos = zoom y desplazar)
//    rueda <pasos> <x> <y>                        la rueda del mouse sobre (x, y)
//    mod [shift] [alt] [ctrl]                     teclas modificadoras mantenidas mientras se usa
//                                                 el mouse (sin nada: se sueltan)
//    tecla <nombre> [tap|down|up]                 una tecla por nombre de SDL ("m", "Right",
//                                                 "Return", "Escape"...); tap = apretar y soltar.
//                                                 '_' = espacio: "Keypad_7", "Left_Ctrl"
//    layoutpordefecto / division <f>              el layout del arranque para el tamanio actual
//                                                 y la division de su raiz
//    verde [<dx> <dy> [cuadros]]                  la tecla verde del N95 (sola o + una flecha)
//    n95 <accion> ...                             el keypad del N95 sobre la Vista 3D: orbitar,
//                                                 zoom, panear, vista, perspectiva, encuadrar,
//                                                 camara (ver W3dCapturas.cpp)
//    caras <n> <n> ...                            en Modo Edicion, elegir esas caras juntas
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
