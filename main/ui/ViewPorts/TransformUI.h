#ifndef TRANSFORMUI_H
#define TRANSFORMUI_H

/**
 * @file TransformUI.h
 * @brief UI COMPARTIDA de un transform en curso (G/R/S): barra de info, entrada numerica,
 *        botones de la toolbar (tilde/cruz/ejes) y teclado numerico tactil.
 *
 * El viewport 3D es la REFERENCIA: esto extrae su mecanica para que el UV editor y el
 * Editor 2D se porten IGUAL sin duplicar codigo. Cada editor con un G/R/S modal propio
 * implementa los hooks Xform* de ViewportBase (ViewPorts.h); lo comun vive aca:
 *   - W3dFmtFloat / TransformUITextoNum / TransformUITexto2D: los textos de la barra de info
 *   - ViewportBase::RenderBarraInfo (def. en TransformUI.cpp): el dibujo de esa barra
 *   - TransformUIClickBarra: tap en la barra de info -> teclado numerico (NumPad)
 *   - TransformUIPuedeConfirmar: una expresion tipeada INVALIDA no deja confirmar
 *   - ToolbarCrearTransform2D / ToolbarSincronizarTransform2D / ToolbarAccionTransform2D:
 *     los botones Confirmar/Cancelar/X/Y de la toolbar durante el transform (roles TBR_* del 3D)
 *   - ToolbarUsaTactil + los colores de los botones (Tb*): compartidos con la toolbar del 3D
 */

#include <string>
#include <vector>

class ViewportBase;
class Button;

// float -> texto portable (sin %f, que no anda en Symbian). Extraido del 3D (era W3dFmtF).
std::string W3dFmtFloat(float v, int dec);

// "[expr|] = valor<unidad>" (o "= ?" si la expresion esta incompleta): la parte compartida
// del texto de la barra cuando hay ENTRADA NUMERICA activa. La usan el 3D, el UV y el 2D.
std::string TransformUITextoNum(const char* unidad);

// texto COMPLETO de la barra de info de un transform 2D (UV editor / Editor 2D).
// modo 1=mover 2=rotar 3=escalar; d1/d2 = delta YA filtrado por el lock; ang en GRADOS;
// fac = factor de escala; ejeLock 0=libre 1=X 2=Y; ejeA/ejeB = nombres ("X"/"Y");
// unidad = sufijo de los valores ("" en UV, " px" en el 2D). Si hay entrada numerica
// activa muestra la expresion (mismo formato que el 3D).
std::string TransformUITexto2D(int modo, float d1, float d2, float ang, float fac,
                               int ejeLock, const char* ejeA, const char* ejeB,
                               const char* unidad);

// tap/click sobre la BARRA DE INFO durante un transform del viewport -> abre el teclado
// numerico en modo transform (NumPadAbrirTransform). true = el toque era de la barra.
bool TransformUIClickBarra(ViewportBase* vp, int mx, int my);

// false si hay una expresion tipeada INVALIDA (ej "1/"): el transform NO se confirma
// (la barra ya muestra "= ?" como feedback). Lo usan los confirmar del UV y el 2D.
bool TransformUIPuedeConfirmar();

// el input de esta sesion es TACTIL? (los botones tilde/cruz solo aparecen ahi, como el 3D)
bool ToolbarUsaTactil();

// colores compartidos de los botones de transform de la toolbar (3D / UV / 2D):
const float* TbEjeColor(int e);  // 0=X rojo, 1=Y verde, 2=Z azul (letra del boton apagado)
const float* TbEjeBg(int e);     // el mismo color atenuado (fondo del boton encendido)
const float* TbRojo();           // cruz de cancelar
const float* TbRojoBg();
const float* TbVerdeBg();        // fondo del tilde de aceptar (accent atenuado)

// crea los botones de transform de un editor 2D (tilde / cruz / X / Y) al final de B,
// con los MISMOS roles del 3D (TBR_Aceptar/TBR_Cancelar/TBR_EjeX/TBR_EjeY). Ocultos al nacer.
void ToolbarCrearTransform2D(std::vector<Button*>& B);

// visibilidad + colores de esos botones. transformando = hay un G/R/S propio en curso;
// ejeLock = 0 libre / 1 X / 2 Y (resalta el eje bloqueado), -1 = transform SIN ejes
// (ej: huesos 2D; X/Y quedan ocultos). Mismas reglas que el 3D: tilde/cruz solo tactil.
void ToolbarSincronizarTransform2D(std::vector<Button*>& B, bool transformando, int ejeLock);

// despacho de un click de toolbar sobre esos roles, via los hooks Xform* del viewport.
// true = el rol era de transform y se atendio (aceptar / cancelar / toggle de eje).
bool ToolbarAccionTransform2D(ViewportBase* vp, int rol);

#endif // TRANSFORMUI_H
