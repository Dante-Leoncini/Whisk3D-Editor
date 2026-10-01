#ifndef OUTLINER_H
#define OUTLINER_H

#include <vector>
#include <set>
#include <string>
#ifndef W3D_SYMBIAN
    #include <SDL2/SDL.h>
    #include "variables.h"
    #include "sdl_key_compat.h"
#endif
#include "ViewPorts.h"
#include "ScrollBar.h"
#include "WithBorder.h"
#include "objects/Objects.h"
#include "objects/ObjectMode.h"
#include "objects/Textures.h"
#include "WhiskUI/draw/rectangle.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/widgets/TextField.h"   // rename EN LINEA de un recurso / una carpeta
#include "io/RecursosProyecto.h"         // W3D_VISTAS: las vistas por recursos

class Mesh; // fila VIRTUAL "Armature 2D" (los huesos 2D viven dentro del mesh, no son objetos)
class Button;

// LAS VISTAS del outliner (su selector de la barra, el del OJO): la ESCENA (el arbol de la raiz
// activa, el de siempre), la BIBLIOTECA (todo el contenido de este .w3d, en un arbol de carpetas) y
// cada LIBRERIA externa vinculada (otro .w3d, de solo lectura: OUT_VISTA_LIBEXT + su indice). En el
// layout va por su CLAVE ("escena", "biblioteca", "lib:<ruta>").
enum { OUT_VISTA_ESCENA = 0, OUT_VISTA_BIBLIOTECA = 1, OUT_VISTA_LIBEXT = 2 };

// UNA FILA de la BIBLIOTECA: una carpeta o un recurso. Es el UNICO modelo de filas de la vista:
// el dibujo (lista o cuadricula), el click, el arrastre, el teclado y el harness lo arman con
// Outliner::FilasRecursos (no hay recorridos paralelos que mantener alineados).
struct OutFilaRec {
    bool carpeta;          // true = fila de carpeta (plegable)
    std::string clave;     // carpeta: su ruta ("A/B"); recurso: su clave de biblioteca ("mallas:Arbol")
    int tipo;              // recurso: su tipo (W3dVistaRec); carpeta: 0
    std::string id;        // recurso: su id en su tipo ("Arbol")
    std::string nombre;    // lo que se ve (el tramo de la carpeta, el nombre del recurso)
    std::string carpetaDe; // recurso: su carpeta; carpeta: su carpeta de arriba
    int  prof;             // sangria
    int  icono;            // IconType
    int  usuarios;         // recurso: cuantos lo usan (0 = HUERFANO); carpeta: cuantos recursos cuelgan
    bool plegada;          // carpeta plegada
    bool soloLectura;      // recurso de solo lectura (una libreria externa)
    bool renombrable;
    bool parte;            // PARTE de otro recurso (un clip de jerarquia debajo de su biblioteca: W3dRecursoItem::padre)
    bool sucio;            // no esta guardado (se dibuja con un '*': CambiosProyecto)
    OutFilaRec() : carpeta(false), tipo(0), prof(0), icono(-1), usuarios(0), plegada(false), soloLectura(false),
                   renombrable(true), parte(false), sucio(false) {}
};

// "Select Users" de la biblioteca (el menu del outliner y el boton de la tarjeta del recurso de
// Properties hacen lo mismo): selecciona en la escena los objetos que usan esos recursos (claves de
// biblioteca) y los muestra (los outliners que miraban la biblioteca vuelven al arbol, centrados).
// 0 = ningun objeto los usa: avisa y la seleccion queda como estaba. Def. en OutlinerRecursos.cpp.
int OutlinerSeleccionarUsuarios(const std::vector<std::string>& claves);

// la MARCA que el arbol de la escena dibuja despues del nombre de 'obj' (un indice de IconType; -1 = ninguna): el
// CANDADO en lo que genera un proxy de una libreria externa (de solo lectura). Def. en Outliner.cpp.
int OutlinerMarcaDeObjeto(Object* obj);

// EL ARRASTRE DE LA BIBLIOTECA hacia afuera del outliner (al viewport 3D, a un desplegable de
// Properties): lo que se arrastra y si es valido donde esta el puntero. Lo dibuja LayoutRenderMenu
// (el icono y el nombre al lado del cursor) y los destinos lo miran para resaltarse. Def. en
// OutlinerRecursos.cpp.
struct OutArrastreBib {
    bool activo;            // hay un recurso de la biblioteca agarrado
    int tipo;               // su tipo (W3dVistaRec)
    std::string id, nombre;
    int icono;
    bool soloLectura;       // de una libreria externa (se arrastra recien con la fase de proxies)
    int x, y;               // el puntero
    int destino;            // lo que pasaria al soltar ahi: 0 nada, 1 valido, -1 invalido
    std::string destinoTexto;   // que haria (en el idioma de la UI): "Assign to the 3D Mesh selector"
    OutArrastreBib() : activo(false), tipo(0), icono(-1), soloLectura(false), x(0), y(0), destino(0) {}
};
extern OutArrastreBib g_outArrastre;
// dibuja el recurso arrastrado junto al puntero (lo llama LayoutRenderMenu, encima de todo)
void OutlinerArrastreRender(int pantallaW, int pantallaH);

class Outliner : public ViewportBase, public WithBorder, public Scrollable {
    public:
        Scrollable* ComoScrollable() { return this; }
        size_t CantidadRenglones;
        int lastContentRows;   // filas visibles del ultimo render: si cambian (import/add/delete/desplegar) se recalcula
                               // el scrollbar (antes solo se recalculaba al redimensionar -> tras importar no scrolleaba)
        Rec2D* Renglon;

        // CULLING vertical: DibujarRenglon/DibujarOjos recorren TODA la jerarquia (avanzan la matriz igual), pero SALTEAN
        // el DRAW (iconos + texto) de las filas fuera del area visible. Sin esto, una escena con muchos objetos dibuja
        // miles de filas/nombres que el Scissor descarta despues (pagando igual los draw-calls). filaDFS = indice de fila
        // en orden DFS (mapea 1:1 con la Y de la matriz); cullBaseY = Y en pantalla de la 1er fila del recorrido actual.
        int cullBaseY;
        unsigned filaDFS;

        Outliner();
        ~Outliner() W3D_OVERRIDE;

        void CalcularRenglon(Object* obj, int* MaxPosXtemp, int* MaxPosYtemp);
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void Render() W3D_OVERRIDE;
        void DibujarRenglon(Object* obj, bool hidden);
        // fila VIRTUAL "Armature 2D" de una malla: UNA sola fila informativa, en AZUL y colgando de
        // la malla, que avisa que el rig 2D es PARTE del mesh y no un objeto de escena. Inerte: no
        // se arrastra, no acepta drops, no se desempareja y NO se despliega (los huesos se listan
        // en la pestania "Armature 2D" del panel Properties).
        void DibujarArm2D(Mesh* m, int idx, bool hidden);
        void DibujarLineaDesplegada(Object* obj);
        void DibujarOjos(Object* obj, bool hidden, bool noRender);

        void button_left() W3D_OVERRIDE;
        void FindMouseOver(int mx, int my);
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE;
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // touch: arrastrar = scroll v/h
#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) W3D_OVERRIDE;
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        void event_key_down(int tecla, bool repeticion) W3D_OVERRIDE;
        void event_key_up(int tecla) W3D_OVERRIDE;
#endif
        // click para seleccionar la fila (compartido; en el N95 lo llama el
        // router del mouse HID, en PC se puede cablear a mouse_button_up)
        void ClickSeleccionar(int mx, int my);

        // numpad "."": scrollea el outliner para CENTRAR
        // la seleccion en la vista (promedio de las filas visibles de lo seleccionado)
        void CentrarSeleccion();
        // scroll MINIMO para que la fila del objeto ACTIVO se vea entera. A diferencia de
        // CentrarSeleccion (numpad "."), no mueve nada si la fila ya esta a la vista: con
        // 315 objetos, centrar en cada tecla marea. La llama la navegacion por flechas.
        void AsegurarVisible();

        // fila bajo el mouse (hover) para feedback visual antes del click
        int hoverFila;
        // arrastre de filas (reordenar / emparentar soltando encima)
        Object* dragObjeto;
        bool dragging;
        int dragY0;
        // vista previa del drop: -2 nada, -1 al vacio (raiz),
        // 0 antes de la fila, 1 hijo de la fila, 2 despues
        int dropFila;
        int dropZona;
        int dropProf; // profundidad (nivel de sangria) del destino: la linea de insercion se indenta a ese nivel
        void SoltarDrag(int mx, int my);

        // MODO MOVER con teclado (sin mouse, clave en N95 para ordenar lamparas antes de los objetos):
        // g (PC) / 1 (Symbian) entra; flechas reordenan (arriba/abajo) o reparentan (izq=sacar / der=meter);
        // OK/Enter confirma; C/backspace cancela (restaura la posicion original en el arbol).
        bool moviendo;
        Object* moverObj;          // el objeto que se esta moviendo (= el activo al entrar)
        Object* moverPadreOrig;    // padre original (NULL = raiz) para cancelar
        Object* moverAnteriorOrig; // hermano que estaba ANTES de moverObj (NULL = era el primero) para cancelar
        void MoverIniciar();       // entra en modo mover con el objeto activo
        void MoverPaso(int dir);   // 0=arriba 1=abajo 2=afuera(unparent) 3=adentro(parent)
        void MoverConfirmar();     // sale del modo (deja la posicion nueva)
        void MoverCancelar();      // restaura la posicion original y sale
        bool ModoMover() const { return moviendo; }
        int ViewportKind() const { return 2; } // (menu de tipo)
        void ClearHover() { hoverFila = -1; } // el mouse se fue

        void key_down_return();

        // ================================================================================
        //  LA BIBLIOTECA (OutlinerRecursos.cpp): el boton de VISTA de la barra (el OJO) elige
        //  que se ve: la ESCENA (el arbol de siempre, todo lo de arriba), la BIBLIOTECA (el
        //  contenido del .w3d: UN arbol de carpetas cosmeticas con todos los tipos mezclados,
        //  un FILTRO por tipo, lista o CUADRICULA con miniaturas) o una LIBRERIA externa (otro
        //  .w3d, de solo lectura). Cada fila de recurso dice si esta EN USO (tilde verde) o
        //  HUERFANO (cruz roja) y cuantos lo usan. Mouse (click, Ctrl/Shift, doble click =
        //  renombrar, arrastrar a una carpeta / al 3D / a Properties, click derecho), tactil
        //  (arrastrar = scroll, MANTENER ~0,5 s = agarrar o el menu contextual), teclado (flechas,
        //  F2, G = mover, Supr) y el keypad del N95 (la barra se navega sin mouse).
        // ================================================================================
        int vista;                 // OUT_VISTA_* (se guarda en el layout como "vista: <clave>")
        int filtro;                // el filtro de la biblioteca: un tipo (W3dVistaRec) o -1 = todo
        bool cuadricula;           // la biblioteca como CUADRICULA de miniaturas (false = lista)
        void CambiarVista(int v);
        void CambiarFiltro(int f);
        void CambiarCuadricula(bool on);
        bool EnBiblioteca() const { return vista != OUT_VISTA_ESCENA; }
        bool VistaSoloLectura() const { return vista >= OUT_VISTA_LIBEXT; }
        // la clave de la vista para el layout ("escena", "biblioteca", "lib:<ruta>") y al reves
        std::string VistaClave() const;
        bool VistaDeClave(const std::string& c);
        // la barra: [0] tipo  [1] VISTA (ojo)  [2] OBJETO  [3] SELECCION  [4] "+"  [5] filtro  [6] lista/cuadricula  [7] raiz
        Button* btnVista;
        Button* btnObjeto;
        Button* btnSeleccion;
        Button* btnNueva;
        Button* btnFiltro;
        Button* btnCuadricula;
        Button* btnRaiz;           // la raiz que se edita (escena / juego / prefab): el mismo menu que el 3D
        std::set<std::string> plegadas;      // carpetas PLEGADAS de la biblioteca (cosmetico)
        std::set<std::string> plegadasExt;   // idem de la libreria externa que se mira
        // el CURSOR de la vista (la fila activa, en verde; teclado y acciones): por IDENTIDAD, no por
        // numero de fila (renombrar/mover/plegar corre los numeros)
        bool cursorEsCarpeta;
        std::string cursorClave;
        // arrastre: un recurso, una CARPETA (con todo lo suyo) o la SELECCION entera, a una carpeta; un
        // recurso tambien al viewport 3D o a un desplegable de Properties (g_outArrastre)
        bool recArrastre, recArrastrando;
        std::string recArrastreId;
        bool recArrastreCarpeta;   // lo que se apreto es una carpeta (recArrastreId = su ruta)
        bool recSoloAlSoltar;      // click sin modificadores sobre una fila de una seleccion de VARIAS:
                                   // si no se arrastra, al soltar queda sola (como en un explorador)
        bool recElegirAlSoltar;    // el click ELIGE la fila (suelta la escena, activa el recurso) al soltar sin
                                   // arrastrar: un arrastre a Properties no puede apagar la tarjeta destino
        int recArrastreY0, recArrastreX0, recDropFila;
        // TACTIL: la fila AGARRADA con una pulsacion larga (arrastrar = mover; soltar sin mover = menu
        // contextual). -1 = nada agarrado.
        int agarreFila;
        bool agarreMovido;
        int agarreX, agarreY;
        // SELECCION MULTIPLE: Ctrl+click agrega/saca, Shift+click = rango desde el cursor, Shift+flechas,
        // "Select All" / "Deselect All" / "Invert Selection", Esc la suelta; sin mouse, el 5 del N95
        // marca/desmarca el cursor. Mover, borrar y "Select Users" van sobre TODA la seleccion (un paso de
        // undo); renombrar y Properties, sobre el cursor. Por IDENTIDAD (claves de biblioteca, rutas de
        // carpeta): plegar, renombrar o mover no la corren. Vacia = el cursor.
        std::set<std::string> selRecursos, selCarpetas;
        // la seleccion la ARMO el usuario (5, Ctrl/Shift, A): el cursor del keypad recorre sin tocarla.
        // false = es la de un click comun o una flecha (sigue al cursor)
        bool selMarcada;
        // RENOMBRAR EN LINEA (doble click, F2, "Rename"): un objeto de la escena, un recurso o una carpeta.
        // El campo se dibuja en la fila; Enter o un click afuera confirman, Esc cancela.
        TextField renombre;
        bool renombrando, renombreCarpeta, renombreObjeto;
        std::string renombreClave;     // la carpeta / la clave del recurso
        unsigned int renombreSerial;   // el objeto (por serial: un objeto borrado no se renombra)
        int renombreVista;
        // MODO MOVER de la biblioteca (G / el 1 del N95, como el de la escena): lo elegido viaja por las
        // carpetas con las flechas (arriba/abajo = la carpeta anterior/siguiente, izquierda = la de arriba,
        // derecha = la primera de adentro), OK/Enter confirma (un paso de undo) y Esc/C cancela.
        bool moviendoRec;
        std::string moverRecDestino;
        // W3dSeleccionSerial que vio la biblioteca: si la ESCENA eligio un objeto despues (en el 3D, en otro
        // outliner, un script), la seleccion y el cursor de la biblioteca se sueltan (o se eligen objetos o
        // se eligen recursos: la fila no puede seguir en verde ni ser lo que borra el Supr)
        unsigned int selEscenaSerial;

        // la lista visible de la vista actual (vacia en la vista Escena)
        void FilasRecursos(std::vector<OutFilaRec>& out);
        int  FilaDelCursor(const std::vector<OutFilaRec>& filas) const;   // -1 = no esta a la vista
        int  FilaRecursoY(int fila) const;       // Y absoluta del CENTRO de la fila (click / harness)
        // LA GEOMETRIA de una fila (lista o cuadricula): su rectangulo en coordenadas del panel (sin
        // PosX/PosY: los suma quien dibuja). La misma cuenta dibuja, clickea y la usa el harness.
        void RectFila(const std::vector<OutFilaRec>& filas, int i, int& rx, int& ry, int& rw, int& rh) const;
        int  FilaEnPunto(const std::vector<OutFilaRec>& filas, int mx, int my) const;   // -1 = ninguna
        int  CeldaTam() const;                   // el lado de una celda de la cuadricula
        // la miniatura que dibuja la celda de un recurso (0 = el icono del tipo: una carpeta, un tipo sin
        // miniatura, o el contenido de una libreria externa, que no se monta)
        unsigned int MiniaturaDeFila(const OutFilaRec& f, int* w, int* h) const;
        bool AbrirMenuDeBarra(int mx, int my) W3D_OVERRIDE; // los desplegables (tambien el hover)
        bool AbrirMenuBoton(int idx);            // idem por indice (teclado del N95)
        int  BotonDelMenuAbierto() const;        // indice del boton cuyo menu esta abierto (-1 = ninguno)
        bool ClickBarra(int mx, int my);         // lo directo de la barra: el boton lista/cuadricula
        bool MenuContexto(int mx, int my);       // click derecho / pulsacion larga: elige la fila y abre Objeto ahi
        bool DobleClick(int mx, int my);         // doble click / doble tap: renombrar el nombre de la fila
        bool PulsacionLarga(int mx, int my);     // tactil: mantener apretado agarra la fila (o menu al soltar)
        bool TeclaRecursos(int tecla);           // LayoutKey (flechas/OK/C del keypad del N95)
        // LayoutKey de un TECLADO DE PC (LayoutTeclaUI: el panel bajo el mouse recibe las flechas y el
        // Enter ANTES que event_key_down): Shift+flechas suma filas, sin Shift la fila queda sola (como
        // en un explorador). En el N95 es TeclaRecursos (el keypad no tiene Shift: las marcas del 5 quedan).
        bool TeclaRecursosPC(int tecla);
        void ElegirFila(const OutFilaRec& f, bool abrirProps); // cursor + recurso activo (suelta la escena)
        // acciones (menus, teclas y harness): operan sobre el cursor / la seleccion
        bool AccionNuevaCarpeta();
        bool AccionNuevoMaterial();
        bool AccionRenombrar();                  // en las dos vistas (en linea)
        bool AccionBorrar(bool confirmar);
        int  AccionPurgar(bool confirmar);
        int  AccionSeleccionarUsuarios();
        bool AccionDuplicarRecurso();
        bool AccionMover(const std::string& carpeta);
        void AbrirMenuMover(int x, int y);       // "Move to Folder" (submenu con las carpetas)
        void RenombreSincronizar();              // confirma el rename en linea si perdio el foco
        void RenombreCancelar();                 // el Esc: descarta sin tocar nada
        bool RenombrarObjetoEnLinea(Object* o);  // la vista Escena: el nombre del objeto en su fila
        // la seleccion multiple
        bool FilaEnSeleccion(const OutFilaRec& f) const;
        int  SeleccionCantidad() const { return (int)(selRecursos.size() + selCarpetas.size()); }
        void SeleccionSolo(const OutFilaRec& f);         // la fila sola (click, flechas)
        void SeleccionAlternar(const OutFilaRec& f);     // Ctrl+click / el 5 del N95
        bool MarcarCursor();                             // el 5 del N95: marca/desmarca la fila del cursor
        bool SoltarSeleccion();                          // Esc / "Deselect All": queda el cursor solo (false = no habia)
        void InvertirSeleccionRec();                     // "Invert Selection"
        // el TOQUE de C del N95 en la biblioteca: borra la seleccion (con confirmacion) como el Supr de PC,
        // en vez de los objetos de la escena. false = vista Escena (sigue su camino).
        bool TeclaBorrarRecursos();
        void SeleccionarTodoRec();                       // A: todas las filas a la vista (o ninguna si ya estaban)
        // lo que mueve / borra / lista una accion: la seleccion (o el cursor, si no hay). 'paraMover'
        // saca lo que ya cuelga de una carpeta elegida (la carpeta lo lleva) y lo de solo lectura.
        // 'recursos' son claves de biblioteca.
        void Objetivo(std::vector<std::string>& recursos, std::vector<std::string>& carpetas, bool paraMover);
        bool MoverObjetivo(const std::string& destino);  // TODO lo elegido adentro de 'destino' (un paso de undo)
        // el modo mover de la biblioteca (ver moviendoRec)
        void MoverRecIniciar();
        void MoverRecPaso(int dir);              // 0 arriba, 1 abajo, 2 afuera, 3 adentro
        void MoverRecConfirmar();
        void MoverRecCancelar();
        // la vista Escena desde el outliner: el menu Seleccion y sus atajos (A / Alt+A / Ctrl+I: 0 todo, 1 nada,
        // 2 invertir) y el menu Objeto con los suyos (Shift+D / Alt+D: duplicar, duplicar vinculado)
        void SeleccionEscena(int accion);
        void DuplicarEscena(bool vinculado);
    private:
        // 'filas' = las de este cuadro (Render las arma una vez)
        void RenderRecursos(int glY, const std::vector<OutFilaRec>& filas);
        void ResizeRecursos();
        void ClickRecursos(int mx, int my);
        void MotionRecursos(int mx, int my);
        void SoltarRecursos(int mx, int my);
        bool TeclaPCRecursos(int tecla);
        // como se mueve la seleccion con el cursor de teclado (ver CursorATecla)
        enum { SEL_MARCAS = 0, SEL_EXTENDER, SEL_SOLA };
        bool TeclaRecursosCon(int tecla, int modoSel);
        void CursorATecla(const std::vector<OutFilaRec>& filas, int n, int modoSel);
        void AsegurarVisibleFila(int fila);
        void SincronizarBarraVista();
};

#endif