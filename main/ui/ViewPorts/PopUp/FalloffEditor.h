#ifndef FALLOFFEDITOR_H
#define FALLOFFEDITOR_H

#include "PopUpBase.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/widgets/Button.h"
#include "edit/W3dFalloff.h"

// ============================================================================
//  EDITOR DE FALLOFF (4 OS) — la lista de presets + la curva editable a mano.
//
//  REUTILIZABLE, igual que el ColorPicker: no sabe de pinceles ni de pesos. Le
//  pasas un W3dFalloff* (el de quien sea) y lo edita en vivo:
//      FalloffEditorOpen(&BrushGet().falloff, x, y);
//  El dueno del dato no tiene que hacer nada mas; los cambios se ven al toque
//  porque se escriben SOBRE su W3dFalloff. Si el dueno se muere antes de que se
//  cierre el popup, llamar ForgetOwner() (mismo contrato que el color picker).
//
//  Contenido, de arriba a abajo:
//    * la LISTA de los 10 tipos (icono + nombre; el activo en verde)
//    * el LIENZO de la curva: la curva dibujada + sus puntos. Solo se puede
//      editar con tipo = Custom; con un preset se ve su forma (de solo lectura),
//      que es la mejor manera de entender que hace cada uno.
//    * con Custom: arrastrar un punto lo mueve, click en el vacio agrega uno,
//      y el boton X borra el seleccionado (los dos extremos no se borran).
//
//  Teclado (Symbian, sin mouse): arriba/abajo recorren la lista, OK elige el
//  tipo. Con Custom, OK sobre el lienzo entra a la curva: izquierda/derecha
//  cambian de punto, y con OK apretado las flechas MUEVEN el punto elegido.
// ============================================================================
class FalloffEditor : public PopUpBase {
    public:
        W3dFalloff* target;      // el falloff que se esta editando (de quien sea)
        W3dFalloff  original;    // copia para cancelar (Esc / C)
        int  selPoint;           // punto de la curva seleccionado (-1 = ninguno)
        int  focus;              // teclado: 0..FoTotal-1 = fila de la lista; FoTotal = lienzo
        bool editingPoint;       // teclado: OK sobre el lienzo -> las flechas mueven el punto
        int  drag;               // 0 = nada, 1 = arrastrando un punto de la curva
        Rec2D* rect;             // dibujo plano (lienzo, curva, puntos)
        Card*  rowCard;          // la caja de cada fila de la lista
        Button* btnDelete;       // X: borra el punto seleccionado (solo Custom)

        // layout (en pixeles del popup, lo calcula Reflow)
        int listY, rowH;
        int canvasX, canvasY, canvasW, canvasH;
        int btnY;

        FalloffEditor();
        ~FalloffEditor();

        void Open(W3dFalloff* Target, int px, int py);
        void Reflow();
        void ForgetOwner();     // el dueno del W3dFalloff se destruye: soltar y cerrar

        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Key(int key) W3D_OVERRIDE;
        void Release() W3D_OVERRIDE;
        bool Dragging() W3D_OVERRIDE;
        void Close() W3D_OVERRIDE;

        // pantalla <-> curva (0..1): el lienzo tiene el 0,0 abajo-izquierda, como un grafico
        void  CurveToScreen(float cx, float cy, int& sx, int& sy) const;
        void  ScreenToCurve(int sx, int sy, float& cx, float& cy) const;
        bool  InCanvas(int mx, int my) const;
        bool  IsCustom() const;  // el lienzo solo se EDITA en Custom (con preset es de lectura)
};

extern FalloffEditor* falloffEditor;

// abre el editor sobre 'target' (crea la instancia si hace falta). Es LA puerta:
// cualquier parte del programa que tenga un W3dFalloff lo edita con esto.
void FalloffEditorOpen(W3dFalloff* target, int px, int py);
// true si el popup activo es el editor de falloff (y esta editando 'target', si se pasa)
bool FalloffEditorActive(const W3dFalloff* target = 0);

#endif // FALLOFFEDITOR_H
