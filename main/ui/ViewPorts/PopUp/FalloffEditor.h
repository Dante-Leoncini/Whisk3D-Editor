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
//      FalloffEditorAbrir(&BrushGet().falloff, x, y);
//  El dueno del dato no tiene que hacer nada mas; los cambios se ven al toque
//  porque se escriben SOBRE su W3dFalloff. Si el dueno se muere antes de que se
//  cierre el popup, llamar OlvidarDueno() (mismo contrato que el color picker).
//
//  Contenido, de arriba a abajo (el mismo orden que el desplegable de Blender,
//  que es de donde viene el vocabulario de los presets):
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
        int  selPunto;           // punto de la curva seleccionado (-1 = ninguno)
        int  foco;               // teclado: 0..FoTotal-1 = fila de la lista; FoTotal = lienzo
        bool editandoPunto;      // teclado: OK sobre el lienzo -> las flechas mueven el punto
        int  arrastre;           // 0 = nada, 1 = arrastrando un punto de la curva
        Rec2D* rect;             // dibujo plano (lienzo, curva, puntos)
        Card*  filaCard;         // la caja de cada fila de la lista
        Button* btnBorrar;       // X: borra el punto seleccionado (solo Custom)

        // layout (en pixeles del popup, lo calcula Reflow)
        int listaY, filaH;
        int lienzoX, lienzoY, lienzoW, lienzoH;
        int btnY;

        FalloffEditor();
        ~FalloffEditor();

        void Abrir(W3dFalloff* Target, int px, int py);
        void Reflow();
        void OlvidarDueno();     // el dueno del W3dFalloff se destruye: soltar y cerrar

        void Render() W3D_OVERRIDE;
        bool Click(int mx, int my) W3D_OVERRIDE;
        bool Motion(int mx, int my) W3D_OVERRIDE;
        bool Tecla(int tecla) W3D_OVERRIDE;
        void Soltar() W3D_OVERRIDE;
        bool Arrastrando() W3D_OVERRIDE;
        void Cerrar() W3D_OVERRIDE;

        // pantalla <-> curva (0..1): el lienzo tiene el 0,0 abajo-izquierda, como un grafico
        void  CurvaAPantalla(float cx, float cy, int& sx, int& sy) const;
        void  PantallaACurva(int sx, int sy, float& cx, float& cy) const;
        bool  EnLienzo(int mx, int my) const;
        bool  EsCustom() const;  // el lienzo solo se EDITA en Custom (con preset es de lectura)
};

extern FalloffEditor* falloffEditor;

// abre el editor sobre 'target' (crea la instancia si hace falta). Es LA puerta:
// cualquier parte del programa que tenga un W3dFalloff lo edita con esto.
void FalloffEditorAbrir(W3dFalloff* target, int px, int py);
// true si el popup activo es el editor de falloff (y esta editando 'target', si se pasa)
bool FalloffEditorActivo(const W3dFalloff* target = 0);

#endif // FALLOFFEDITOR_H
