#ifndef MENUTEXTO_H
#define MENUTEXTO_H
// ============================================================================
//  EL MENU DEL TEXTO: Cortar, Copiar, Pegar y Seleccionar todo. Sale con el click derecho o manteniendo apretado (el
//  dedo) sobre una caja de texto enfocada (un nombre, un numero que se esta tipeando, el nombre del archivo del
//  explorador...) o sobre el codigo del IDE. Las mismas acciones que Ctrl+X / Ctrl+C / Ctrl+V / Ctrl+A
// ============================================================================
class IDE;
// el click derecho (controles): sobre el campo enfocado o sobre el codigo de un IDE. true = abrio el menu
bool MenuTextoClickDerecho(int mx, int my);
// sobre el campo enfocado, en (menuX, menuY) (la pulsacion larga de Properties: el menu donde estaba el circulo)
bool MenuTextoEnCampo(int mx, int my, int menuX, int menuY);
// sobre el IDE (la lupa se solto sin mover, o el click derecho): el cursor va ahi si no cae en la seleccion
bool MenuTextoIDE(IDE* ide, int mx, int my);
// el menu del texto esta abierto (un click en el no tiene que confirmar el numero que se esta tipeando)
bool MenuTextoAbierto();
#endif
