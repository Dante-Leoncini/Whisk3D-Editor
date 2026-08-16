#ifndef W3DT9_H
#define W3DT9_H

#include <e32def.h>

// ============================================================================
//  T9 / MULTI-TAP del keypad para editar campos de texto en el N95 (sin teclado).
//  Se activa SOLO cuando hay un campo de texto enfocado (g_textFieldActivo != NULL,
//  de WhiskUI/TextField.h). Modos: Abc (1a mayuscula) / abc / ABC / 123 (numerico),
//  se ciclan con '#'. Cada tecla 0-9 cicla su set con multi-tap; sin apretar 1 seg
//  el caracter se confirma. Un teclado BT/QWERTY escribe directo (no pasa por aca).
// ============================================================================

TBool       W3dT9Activo();                  // hay un campo de texto en edicion?
TBool       W3dT9Tecla(TInt aScan, TBool aDown); // tecla del keypad mientras se edita. ETrue = manejada.
void        W3dT9Tick();                    // por frame: confirma el char pendiente al vencer el timeout
const char* W3dT9ModoTexto();               // "Abc"/"abc"/"ABC"/"123" (o "" si no se edita). Para la barra.
void        W3dT9Reset();                   // confirmar/soltar el char pendiente (cambio de campo, OK, cancel)
TBool       W3dT9HayPendiente();            // hay un char a medio elegir? (para el guioncito bajo el cursor)

#endif // W3DT9_H
