#ifndef NUMINPUT_H
#define NUMINPUT_H

/**
 * @file NumInput.h
 * @brief Entrada numerica / formulas durante un transform + evaluador de expresiones. Extraido de LayoutInput.
 *
 * Entrada NUMERICA / formulas durante un transform (tipear un valor exacto) + evaluador de expresiones.
 * Extraido de LayoutInput. El estado (gNum*) es file-local en NumInput.cpp; esto es la interfaz.
 */
#include <string>

bool W3dEvalExpr(const std::string& str, float& out);   // evalua "2+3*4" (formulas en el input)
bool TextFieldInputChar(int c);       // rutea un caracter tipeado al TextField activo
bool NumInputChar(int c);             // true = lo consumio (hay transform activo)
bool NumInputActivo();                // hay una expresion tipeada en curso
void NumInputReset();                 // limpia (al terminar/cancelar el transform)
const std::string& NumInputBuffer();  // la expresion (para la barra de estado)
int  NumInputCaret();                 // posicion del caret
bool NumInputNegado();                // el signo '-' esta activo
bool NumInputValor(float& out);       // valor evaluado (false si la expresion esta incompleta)
void NumInputLeft();  void NumInputRight();   // mover el caret (teclado tactil)
void NumInputBegin();                 // activa la entrada
void NumInputConfirmar();             // OK del teclado tactil
void NumInputCancelar();              // X del teclado tactil
bool NumInputTransformEnCurso();      // hay un transform activo

#endif
