#ifndef LAYOUTINPUT_H
#define LAYOUTINPUT_H

/*
 * Ruteo de entrada COMPARTIDO (4 OS) sobre el arbol de viewports
 * (rootViewport): menus desplegables, barras de botones, clicks en los
 * paneles, teclado por hover y el pick 3D por color. Cada plataforma
 * solo traduce sus eventos nativos a estas llamadas.
 */

#include "ViewPorts.h"
#include "WhiskUI/PopupMenu.h"
#include <string>

class Quaternion; // para la firma del trackball (EditXformRotAbs)
class Mesh;       // para la firma de MergeVertsEdit (Mesh* + Vector3&)

// teclas abstractas (cada plataforma traduce las suyas)
//  Enter  = OK/activar el elemento enfocado (abre carpeta, etc.)
//  Cancel = volver/cerrar (en Symbian: tecla soft IZQUIERDA del File browser)
//  Accept = confirmar la accion principal (en Symbian: soft DERECHA = Import)
struct LayoutKey {
    enum Enum { Up, Down, Left, Right, Enter, Cancel, Accept };
};

// true = la UI (menu/barra/panel) consumio el click; false = es de la
// escena 3D (cada plataforma decide: pick, transform, etc.)
bool LayoutClickUI(int mx, int my);

// hover de menus/barras + viewport 3D activo; true = lo consumio un menu
bool LayoutMotionUI(int mx, int my);

// teclado: menu abierto > edicion de propiedad > panel bajo el mouse
bool LayoutTeclaUI(int tecla, int mx, int my);
bool LayoutPopupRepeat(int tecla); // flecha MANTENIDA al popup activo (solo ajusta valores, no navega)

// rutea una flecha/OK al viewport ACTIVO (sin mouse): propiedades/outliner.
// El 3D devuelve false (lo maneja orbit/transform). Keyboard-solo (Symbian).
bool LayoutTeclaPanelActivo(int tecla);
bool LayoutUVNavFrame(int dx, int dy, bool zoomMode); // editor UV: paneo constante (o zoom si 0) por flecha mantenida

bool LayoutMenuAbierto();

// cambia el viewport activo (borde verde) a la siguiente hoja (dir=+1) o la
// anterior (-1), dando la vuelta. Sin mouse es la unica forma de elegirlo.
void LayoutCiclarViewportActivo(int dir);

// abre/cierra la barra de menu del viewport activo (soft-izquierda en Symbian)
void LayoutToggleBarraViewportActivo();

// redimensiona el viewport activo en un eje (dx izq/der, dy arr/ab) ajustando
// el divisor del ancestro correcto. verde+flechas en Symbian.
void LayoutRedimensionarViewportActivo(int dx, int dy, float paso);

// arrastre de la barra de scroll: true si hay una agarrada; Soltar la
// suelta (PC lo llama al soltar el boton izquierdo)
bool LayoutEnArrastre();
bool LayoutPopupArrastrando(); // el picker arrastrando (cursor violeta)
void LayoutSoltar(int mx, int my);

// menus de emparentar: ctrl+P (Set Parent To) / alt+P (Clear Parent)
void LayoutMenuParent(bool clear, int mx, int my);

// menu de snap: shift+S (mueve seleccion / cursor 3D, estilo Blender)
void LayoutMenuSnap(int mx, int my);

// menu ADD en el cursor: shift+A (object mode). Mismo MenuAdd de la barra.
void LayoutMenuAdd(int mx, int my);

// menu DELETE de Edit Mode (X / Backspace): sale cerca de (mx,my). Devuelve true
// si lo abrio (estamos en Edit Mode); false en Object Mode (el caller borra objetos).
bool LayoutDeleteEdit(int mx, int my);
void LayoutApplyMenu(int mx, int my); // Ctrl+A (Object Mode): abre el menu Apply (Location/Rotation/Scale/All) en el cursor
void LayoutMaximizar();      // maximiza/restaura el viewport activo a pantalla completa (toggle, no destructivo)
bool LayoutEstaMaximizado(); // hay un viewport en fullscreen?

// menu TRANSFORM PIVOT POINT (objeto + edit): sale cerca de (mx,my). Setea el
// pivote (g_transformPivot) que usan rotar/escalar + el checkbox Lock Normals.
void LayoutMenuPivot(int mx, int my);

// ===== TRANSFORM de sub-elementos en EDIT MODE (G/R/S sobre verts/aristas/caras) =====
// Mueve/rota/escala los VERTICES seleccionados de la malla en edicion (no el
// objeto). Reusa el estado del transform de objetos (estado/axisSelect/orientacion/
// pivot/cam). El dispatch del viewport ramifica aca cuando InteractionMode==EditMode.
bool EditXformStart(int est, int eje); // arranca un transform de malla (G/R/S): setea estado/eje + captura undo
bool EditXformActivo();              // hay un transform de malla en curso
void EditXformIniciar();            // snapshot de la seleccion (lo llaman los starters G/R/S)
void ClipMirrorReset();             // (def. en MeshEdit.cpp) olvida los verts "pegados" al plano del mirror clipping
// (def. en MeshEdit.cpp) MERGE de la seleccion: 0 At Center, 1 At Cursor (cursorLocal), 2 Collapse, 3 By Distance(<dist)
void MergeVertsEdit(Mesh* m, int modo, float dist, const Vector3& cursorLocal);
void EditXformReiniciar();          // restaura al snapshot (cambio de eje X/Y/Z)
void EditXformConfirmar();          // fija: recalcula bordes + normales (salvo Lock Normals)
void EditXformCancelar();           // descarta: restaura el snapshot
void EditXformIniciarExtrude(const Vector3& normalLocal); // move de la tapa del extrude
void EditXformNumValor(float v); // entrada numerica: aplica el valor exacto (malla)
void LayoutExtrudeFaces(); // E: extruye la seleccion (vert/arista/cara) + arranca el move
void LayoutDuplicarEdit(); // Shift+D en edit: duplica la seleccion + move libre
void LayoutRipEdit();      // V en edit: separa la malla a lo largo de la seleccion
void LayoutNewFaceEdit();  // F: crea arista/cara desde los verts seleccionados
void LayoutShade(bool smooth); // Face > Shade Smooth/Flat (redondea/aplana)
void LayoutRecalcNormales();   // Face > Recalculate Normals (re-orienta hacia afuera + panel Inside)
void LayoutTriangulate();      // Face > Triangulate Faces (Ctrl T): parte las caras sel de >3 lados en tris
void LayoutMarkSharp(bool sharp);     // Edge > Mark/Clear Sharp (borde afilado en malla smooth)
void LayoutMenuSharp(int mx, int my); // tecla W: menu Mark/Clear Sharp en el cursor
void LayoutMenuUV(int mx, int my);    // tecla U: menu UV (Mark Seam + proyecciones) en el cursor
void LayoutSelectLinked(int mx, int my); // L: selecciona la isla conexa bajo el mouse
// navegacion de seleccion por teclado en Edit Mode (lapiz + flechas de Symbian)
bool EditSelAvanzar(int paso, bool extender); // lapiz/flecha: avanza el activo (+1/-1); extender=mantiene
bool EditSelTodoToggle();                     // lapiz+arriba: todo <-> nada
bool EditSelToggleActual();                   // lapiz+abajo: togglea el indice activo

// ===== LOOP CUT AND SLIDE (Ctrl+R) — modal compartido 4 OS =====
bool  LoopCutActivo();                   // hay un loop cut en curso (preview o slide)
void  LoopCutIniciar(int mx, int my);    // Ctrl+R: entra al modal (fase preview)
void  LoopCutMotion(int mx, int my);     // motion: sigue la arista (preview) / ajusta factor (slide)
void  LoopCutWheel(int dir);             // rueda: +/- cortes (solo en preview)
void  LoopCutClickIzq(int mx, int my);   // click izq: aplica+slide / confirma
void  LoopCutClickDer();                 // click der: factor 0 + confirma
void  LoopCutCancelar();                 // Esc: descarta
void  LoopCutRedoAplicar(int cortes, float factor); // panel redo: re-corta desde el snapshot
int   LoopCutGetCortes();
float LoopCutGetFactor();
void  LoopCutTecla(int dir);             // flechas en el modal (0=izq 1=der 2=arriba 3=abajo)
bool  LoopCutOrientando();               // true en la fase de elegir orientacion del quad
void  LayoutLoopCutDesdeActivo();        // menu Edge/Face: loop cut sobre el borde/quad ACTIVO
void  LoopCutRenderPreview();            // dibuja el preview (lo llama el viewport 3D)

// Loop Select (menu Select). tipo: 0=Edge Loop, 1=Edge Ring (modo borde), 2=Face Loop (modo cara).
// Edge: opera sobre el borde ACTIVO (direccion obvia). Face: arranca un modal de direccion
// (las flechas eligen 1 de las 2; OK/click acepta) porque una cara tiene 2 sentidos posibles.
void  LayoutLoopSelectActivo(int tipo);
bool  LoopSelOrientando();               // true en el modal de elegir direccion (desde una cara)
void  LoopSelTecla(int dir);             // flechas en el modal (0/2 = sentido A, 1/3 = sentido B)
void  LoopSelConfirm();                  // OK / click / enter: acepta la seleccion actual
// loop select en una posicion (mouse virtual de Symbian: lapiz+OK). false si no pickeo.
bool  LayoutLoopSelectEnPos(int mx, int my, int vx, int vy, int vw, int vh, int screenH);

// Pick Shortest Path guiado (menu Select): 2 clicks con cartel-tutorial. fill = rellena la region.
// El click se intercepta en ScenePick3D -> anda igual en PC (mouse) y Symbian (mouse virtual+OK).
void  LayoutPickPathIniciar(bool fill);
void  LayoutPickPathCancelar();          // ESC / click der: sale del modo guiado
bool  PickPathGuiado();                  // true mientras espera los 2 clicks
// modo guiado de 1 click: Select Linked (desde el menu) y Loop Select sin elemento activo
void  LayoutSelectLinkedGuiado();        // menu Select > Select Linked: pide click sobre el elemento
void  LayoutGuiadoCancelar();            // ESC / click der: sale del modo guiado de 1 click
bool  GuiadoUnClickActivo();             // true mientras espera el click
// cartel-tutorial persistente (azul, sin 'x', no expira); lo usa el modo guiado
void  NotificarHint(const std::string& msg);
void  NotificarHintClear();
// menu de contexto de Edit Mode (W / boton de barra): abre el menu Vertex/Edge/Face
// segun el sub-modo, en (mx,my). Los menus son separados (no submenus de "Mesh").
void LayoutMenuEditContexto(int mx, int my);
// submenus reutilizables para embeber en los menus de barra construidos en ViewPort3D.cpp
// (el menu "Mesh" de Edit Mode: Transform arriba, Snap y Delete; y el menu Object: Set/Clear Parent):
PopupMenu* LayoutSubmenuSnap();        // Snap (cursor/seleccion) -> menu Mesh
PopupMenu* LayoutSubmenuDelete();      // Delete (vertices/aristas/caras/loops) -> menu Mesh
PopupMenu* LayoutSubmenuMerge();       // Merge (At Center/Cursor/Collapse/By Distance) -> menu Mesh
void LayoutMenuMerge(int mx, int my);  // tecla M en Edit Mode: abre el menu Merge en el cursor
PopupMenu* LayoutSubmenuSetParent();   // Set Parent To -> menu Object
PopupMenu* LayoutSubmenuClearParent(); // Clear Parent  -> menu Object

// ===== ENTRADA NUMERICA / FORMULAS durante un transform (COMPARTIDA 4 OS) =====
// Cada plataforma alimenta los caracteres tipeados por NumInputChar (PC: SDL_TEXTINPUT;
// Symbian: su teclado). Numeros, '.', '(' ')' '*' '/' '+'; '-' alterna signo; 8=backspace.
bool NumInputChar(int c);          // true = lo consumio (hay transform activo)
bool NumInputActivo();             // hay una expresion tipeada en curso
void NumInputReset();              // limpia (al terminar/cancelar el transform)
const std::string& NumInputBuffer(); // la expresion (para la barra de estado)
bool NumInputNegado();             // el signo '-' esta activo
bool NumInputValor(float& out);    // valor evaluado (false si la expresion esta incompleta)

// caja de texto editable (reutilizable): el struct + el ruteo viven en UI/TextField.h
#include "WhiskUI/TextField.h"
// aplican un paso del transform (dx,dy de pantalla), igual que las de objetos
void EditXformTraslacion(int dx, int dy, float speed);
void EditXformRotEje(int dx, int dy);
void EditXformRotOrbital(int dx, int dy);
void EditXformRotAbs(const Quaternion& qAbs); // trackball (delta absoluto desde el inicio)
void EditXformScale(int dx, int dy, float factor);
// valores acumulados para la barra de estado (Translate/Scale; la rotacion usa gAnguloTransform)
Vector3 EditXformTransDelta();
float   EditXformScaleFactor();

// dibuja el desplegable abierto encima de todo (ortho de pantalla)
void LayoutRenderMenu(int screenW, int screenH);

// NOTIFICACIONES (toasts) — abajo a la izquierda, alineadas con el log. Exito (verde)
// se cierran solas; error (cruz roja) quedan hasta la 'x'. Compartido 4 OS.
void Notificar(const std::string& msg, bool error); // agrega una (y la loguea)
void NotificacionesTick(float dt);                  // 1x por frame: expira las de exito
void NotificacionesRender(int screenW, int screenH);// dibuja encima de todo
bool NotificacionesClick(int mx, int my);           // 'x' de error: cierra. true si consumio
void NotificacionesMotion(int mx, int my);          // hover de la 'x' (gris->blanca)

// FPS: llamar 1x por frame con el reloj de pared en ms (cada plataforma el suyo)
void LayoutTickFPS(unsigned long wallMs);

// Tab: alterna Object<->Edit Mode si el activo es una malla (true si alterno)
bool LayoutToggleEditMode();

// deriva g_editMesh del modo + objeto activo. Llamar al cambiar de modo/objeto.
// COMPARTIDA PC+Symbian (antes solo la corria el render de PC -> Symbian quedaba sin edicion).
void ActualizarEditMeshActivo();

// seleccion 3D por color picking (compartida); (mx,my) en pantalla,
// (vx,vy,vw,vh) el rect del viewport 3D, arbol arriba-izquierda
bool ScenePick3D(int mx, int my, int vx, int vy, int vw, int vh, int screenH);

// mover el cursor REAL del mouse (PC: SDL_WarpMouse; Symbian: el
// cursor virtual); lo usan los drags que no deben salirse de un rect
extern void (*LayoutWarpMouse)(int x, int y);

// importar OBJ: el dialogo de archivos es de cada plataforma
extern void (*LayoutImportObj)();
// el arbol cambio (cambio de tipo / expand / split): la plataforma
// refresca sus punteros cacheados si los tiene
extern void (*LayoutArbolCambiado)();

#endif
