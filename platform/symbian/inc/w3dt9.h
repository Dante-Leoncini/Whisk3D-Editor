#ifndef W3DT9_H
#define W3DT9_H

#include <e32def.h>

// ============================================================================
//  T9 / MULTI-TAP del keypad para editar campos de texto en el N95 (sin teclado).
//  Se activa SOLO cuando hay un campo de texto enfocado (g_activeTextField != NULL,
//  de WhiskUI/TextField.h). Modos: Abc (1a mayuscula) / abc / ABC / 123 (numerico),
//  se ciclan con '#'. Cada tecla 0-9 cicla su set con multi-tap; sin apretar 1 seg
//  el caracter se confirma. Un teclado BT/QWERTY escribe directo (no pasa por aca).
// ============================================================================

TBool       W3dT9Active();                  // hay un campo de texto en edicion?
TBool       W3dT9Key(TInt aScan, TBool aDown); // tecla del keypad mientras se edita. ETrue = manejada.
void        W3dT9Tick();                  // por frame: confirma el char pendiente al vencer el timeout
const char* W3dT9TextMode();              // "Abc"/"abc"/"ABC"/"123" (o "" si no se edita). Para la barra.
void        W3dT9Reset();                 // confirmar/soltar el char pendiente (cambio de campo, OK, cancel)
TBool       W3dT9HasPending();            // hay un char a medio elegir? (para el guioncito bajo el cursor)

// El editor de scripts (IDE) tambien recibe el T9: la plataforma (w3dlayout) apunta estos hooks al IDE ACTIVO.
// Con W3dT9IDEEditing()==true el T9 escribe en el IDE en vez de en g_activeTextField. NULL = no hay IDE.
extern bool (*W3dT9IDEEditing)();
extern void (*W3dT9IDEInsert)(int c);
extern void (*W3dT9IDEDelete)();
extern bool (*W3dT9IDEStartWord)();

#endif // W3DT9_H
