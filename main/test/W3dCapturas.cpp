// Comandos de harness para las capturas de los tutoriales (ver W3dCapturas.h y tools/capturas/README.md).
#include "test/W3dCapturas.h"
#include "test/W3dHistorial.h"        // W3dHistorialCuadro (la grabadora de pasos, tambien en el harness)
#include "app/controles.h"            // SDL_Event + InputUsuarioSDL3: la puerta real de la entrada
#include "app/variables.h"            // window
#include "ViewPorts/ViewPorts.h"      // ViewportBase / ViewportColumn (el arbol del layout)
#include "ViewPorts/ViewPort3D.h"     // Viewport3D (layoutpordefecto)
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/core/UI.h"          // RenglonHeightGS, gapGS, borderGS (las filas del menu)
#include "render/OpcionesRender.h"    // g_redraw
#include "objects/Objects.h"          // g_editMesh, EditSelectMode (caras)
#include "objects/Mesh.h"             // Mesh (caras)
#include "edit/Modifier.h"            // Modifier (modeje)
#include "objects/Empty.h"            // el dibujo del Vacio (vaciodibujo)
#include "animation/Animation.h"      // las curvas de los keyframes (keyinterp)
#include "script/W3dScript.h"         // W3dScriptDatos / W3dScriptEntrada (scriptval: los valores de un script)
#include "physics/W3dRigido.h"        // W3dRigidoDef (objfisica)
#include "objects/Armature.h"         // tiraesqueleto / esqueletoclip
#include "animation/SkeletalAnimation.h"   // W3dArmatureClipPorNombre (tiraesqueleto)
// el centro del ojo de un script de la lista (Properties.cpp): capinfo y ojoscript
class Properties;
bool PropsOjoScriptCentro(Properties* p, int fila, int* cx, int* cy);
class Button;
Button* PropsAyudaEn(Properties* p, int mx, int my);   // el cartelito de ayuda de Propiedades (Properties.cpp)
#include "WhiskUI/text/font.h"         // WhiskFont->Soporta (cobertura)
#include "config/W3dLang.h"           // los idiomas (cobertura)
#include "w3dFilesystem.h"            // leer res/idiomas (cobertura)
#include <set>
#include "objects/EditMesh.h"         // EditMesh (caras)
#include <stdio.h>
#include <string.h>
#include <map>
#include <math.h>
#include "importers/stl.h"         // ExportSTL (stl)
#include "objects/ObjectMode.h"     // SetPosicion / SetRotacion / SetScale / CiclarEjeTransform (n95 transformar)
#include "ViewPorts/Gizmo.h"         // GizmoManijaEnPantalla (capinfo: las manijas)
#include "importers/import_obj.h"   // CargarTodasTexturasPendientes (texturas)
#include "undo/Undo.h"               // UndoConAgregar / UndoModAgregar (agregar)
#include "ViewPorts/Outliner.h"      // las filas (capinfo)
#include "ViewPorts/PopUp/FileBrowser.h"   // el explorador de archivos (capinfo: sus archivos y botones)
#include "ViewPorts/PopUp/GuardarComoPopup.h"   // guardarcomofijar: llenar el "Guardar como..." sin guardar
#include "ViewPorts/Timeline.h"      // la linea de tiempo (tl, tlframes)
#include "ViewPorts/Properties.h"    // las tarjetas y filas (capinfo)
#include "ViewPorts/PopUp/NumPad.h"  // NumPadTeclaCentro (capinfo)
#include "WhiskUI/widgets/Tab.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
extern int OutlinerFilaDeObjeto(Object* o);   // Outliner.cpp: la fila visible de un objeto en la escena (-1)

extern ViewportBase* rootViewport;
extern int MenuPantallaW, MenuPantallaH;
// la tecla VERDE del N95 (platform/symbian/src/Whisk3DContainer.cpp): las MISMAS funciones que llama el telefono
void LayoutCiclarViewportActivo(int dir);                              // verde sola (al soltarla)
void LayoutRedimensionarViewportActivo(int dx, int dy, float paso);    // verde + flecha, una vez por cuadro
ViewportBase* LayoutPorDefecto(int w, int h, Viewport3D** vp3dOut);    // LayoutInput.cpp: el del arranque
extern Viewport3D* Viewport3DActive;
extern bool LShiftPressed, LAltPressed, LCtrlPressed;   // controles.cpp (los modificadores que mira el viewport 3D)
void LayoutRenderMenu(int w, int h);
void SetGlobalScale(int scale);        // ViewPorts.cpp: re-deriva las metricas *GS   // LayoutInput.cpp: los desplegables y popups (van encima de todo)

namespace {
    // la ultima posicion de cada puntero: SDL manda el desplazamiento (xrel/yrel) en cada movimiento, y el
    // arrastre del boton de la esquina redimensiona con ESO, no con la posicion
    int gRatonX = 0, gRatonY = 0;
    Uint32 gRatonBotones = 0;
    std::map<int, std::pair<int, int> > gDedos;   // la ultima posicion de cada dedo apoyado (por id)
    int gModoN95 = 0;   // el transform del keypad en curso (n95 transformar): 0 nada, 1 mover, 2 rotar, 3 escalar

    // UN CUADRO, como el loop del editor entre evento y evento: el hit-test de los botones usa la posicion
    // que les dejo el ultimo dibujado (sin esto, el primer click no le acertaba a la esquina: sx/sy en 0)
    void Cuadro() {
        if (!rootViewport) return;
        // como el loop del editor (main.cpp): los keyframes se aplican al cuadro actual. Sin esto, un click en los
        // numeros de la Linea de tiempo cambiaba el cuadro pero el objeto seguia con la pose del cuadro anterior (y una
        // S despues escalaba esa pose). Solo trabaja al cambiar de cuadro
        ::AplicarAnimacionObjetos();   // (ObjectMode.h)
        rootViewport->Render();
        LayoutRenderMenu(MenuPantallaW, MenuPantallaH);
#ifdef W3D_MCP
        W3dHistorialCuadro();   // la grabadora de pasos (W3D_HISTORIAL): como en el loop del editor
#endif
    }

    bool gPrimero = true;
    void Despachar(SDL_Event& e) {
        if (gPrimero) { gPrimero = false; Cuadro(); }
        g_redraw = true;
        InputUsuarioSDL3(e);
        Cuadro();
    }

    int BotonDe(const std::string& s) {
        if (s == "der" || s == "derecho") return SDL_BUTTON_RIGHT;
        if (s == "medio") return SDL_BUTTON_MIDDLE;
        return SDL_BUTTON_LEFT;
    }

    // el editor lee la posicion del mouse de SDL (SDL_GetMouseState) al apretar un boton (GuardarMousePos): el delta
    // del arrastre se mide desde ahi. Los eventos sintetizados no la tocan: se lleva el cursor de SDL a mano (en el
    // driver 'offscreen' solo cambia ese estado). Sin esto el primer movimiento de un arrastre venia desde (0, 0)
    // y la vista pegaba un salto enorme.
    void LlevarCursor(int x, int y) { if (window) SDL_WarpMouseInWindow(window, (float)x, (float)y); }

    void Movio(Uint32 which, Uint32 estado, int x, int y, int px, int py) {
        LlevarCursor(x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEMOTION;
        e.motion.timestamp = SDL_GetTicks();
        e.motion.which = which;
        e.motion.state = estado;
        e.motion.x = x; e.motion.y = y; e.motion.xrel = x - px; e.motion.yrel = y - py;
        Despachar(e);
    }

    int gClicks = 1;   // 'raton doble': el 2do click llega con clicks = 2 (SDL), que es lo que controles.cpp mira
    void Boton(Uint32 which, int b, bool abajo, int x, int y) {
        LlevarCursor(x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = abajo ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.timestamp = SDL_GetTicks();
        e.button.which = which;
        e.button.button = (Uint8)b;
        e.button.state = abajo ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = (Uint8)gClicks;
        e.button.x = x; e.button.y = y;
        Despachar(e);
    }

    void Dedo(Uint32 tipo, int id, int x, int y, int px, int py) {
        int ww = 0, hh = 0; if (window) SDL_GetWindowSize(window, &ww, &hh);
        if (ww <= 0 || hh <= 0) { ww = MenuPantallaW; hh = MenuPantallaH; }
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = tipo;
        e.tfinger.timestamp = SDL_GetTicks();
        e.tfinger.fingerId = (SDL_FingerID)id;
        // controles.cpp lo pasa a pixeles con el tamanio de la VENTANA: se normaliza con el mismo
        // (+0.5: que el (int) de vuelta caiga en el mismo pixel)
        e.tfinger.x = ((float)x + 0.5f) / (float)ww;  e.tfinger.y = ((float)y + 0.5f) / (float)hh;
        e.tfinger.dx = (float)(x - px) / (float)ww;   e.tfinger.dy = (float)(y - py) / (float)hh;
        e.tfinger.pressure = (tipo == SDL_FINGERUP) ? 0.0f : 1.0f;
        Despachar(e);
    }

    // la rueda va al viewport bajo el cursor, y controles.cpp lo lee de SDL_GetMouseState: se lleva el
    // cursor ahi primero (en el driver 'offscreen' no hay cursor de verdad: solo cambia el estado de SDL)
    void Rueda(int pasos, int x, int y) {
        if (window) SDL_WarpMouseInWindow(window, x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEWHEEL;
        e.wheel.timestamp = SDL_GetTicks();
        e.wheel.which = 0;
        e.wheel.y = pasos;
        Despachar(e);
    }

    // la misma cuenta que el harness del Outliner (W3dPruebasOutliner.cpp, PuntoDeNombre): el centro del NOMBRE de la fila
    void InfoFilas(Outliner* o, Object* raiz) {
        if (!raiz) return;
        for (size_t i = 0; i < raiz->Childrens.size(); i++) {
            Object* ob = raiz->Childrens[i];
            const int fila = OutlinerFilaDeObjeto(ob);
            if (fila >= 0) {
                int prof = 0;
                for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
                const int x = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
                const int y = o->y + borderGS + o->PosY + o->BarTopOffset() + fila * (int)RenglonHeightGS + (int)RenglonHeightGS / 2;
                const int reserva = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;   // (Outliner.cpp: la barra de desplazamiento)
                const int ojo = o->x + o->width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reserva + IconSizeGS / 2;
                const int cam = o->x + o->width - IconSizeGS - marginGS - borderGS - reserva + IconSizeGS / 2;
                const int flecha = o->x + marginGS + o->PosX + prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
                if (y >= o->y && y < o->y + o->height)
                    printf("@cap fila x=%d y=%d prof=%d ojo=%d camara=%d flecha=%d elegido=%d nombre=%s\n", x, y, prof, ojo, cam, flecha,
                           ob->select ? 1 : 0, ob->name.c_str());
            }
            InfoFilas(o, ob);
        }
    }

    void InfoObjetos(Viewport3D* vp, Object* raiz) {
        if (!raiz) return;
        for (size_t i = 0; i < raiz->Childrens.size(); i++) {
            Object* ob = raiz->Childrens[i];
            float sx = 0, sy = 0;
            if (ob->getType() != ObjectType::collection && vp->ProyectarPunto(ob->GetGlobalPositionBase(), sx, sy))
                printf("@cap objeto x=%d y=%d elegido=%d nombre=%s\n", vp->x + (int)(sx + 0.5f), vp->y + (int)(sy + 0.5f),
                       ob->select ? 1 : 0, ob->name.c_str());
            InfoObjetos(vp, ob);
        }
    }

    // el nombre de una curva en las recetas -> la Interpolation del keyframe (-1 = no existe)
    int InterpDeNombre(const std::string& q) {
        if (q == "lineal") return KfLinear;
        if (q == "bezier") return KfBezier;
        if (q == "in") return KfInterpCon(KfCubic, KfEaseIn);
        if (q == "out") return KfInterpCon(KfCubic, KfEaseOut);
        if (q == "inout") return KfInterpCon(KfCubic, KfEaseInOut);
        if (q == "cuadratica") return KfQuad;                 // (Automatica: Al arrancar)
        if (q == "rebote") return KfBounce;                   // (Automatica: Al llegar)
        if (q == "elastica") return KfElastic;                // (Automatica: Al llegar)
        return -1;
    }
    void InfoHojas(ViewportBase* n) {
        if (!n) return;
        if (!n->isLeaf()) {
            InfoHojas(((ViewportColumn*)n)->childA);
            InfoHojas(((ViewportColumn*)n)->childB);
            return;
        }
        n->ActualizarBarra();
        int bx = -1, by = -1, bw = 0, bh = 0;
        if (!n->BarButtons.empty()) {
            Button* b = n->BarButtons[0];
            bx = b->sx; by = b->sy; bw = b->width; bh = b->height;
        }
        printf("@cap vp kind=%d x=%d y=%d w=%d h=%d esquina=%d,%d,%d,%d barra=%d botones=%d activo=%d\n",
               n->ViewportKind(), n->x, n->y, n->width, n->height, bx, by, bw, bh,
               n->BarHeight(), (int)n->BarButtons.size(), n == viewPortActive ? 1 : 0);
        // el boton del menu View de la Vista 3D (en la PC es solo un icono: no se lo encuentra por el texto)
        if (n->ViewportKind() == 1) {
            Button* bv = BarRolBtn(n->BarButtons, BR_View);
            if (bv && bv->visible) printf("@cap botonvista x=%d y=%d w=%d h=%d\n", bv->sx, bv->sy, bv->width, bv->height);
        }
        // el candado de la vista de camara (si se ve): para apuntarle con el mouse o el dedo
        if (n->ViewportKind() == 1 && ((Viewport3D*)n)->CandadoCamaraVisible()) {
            int rx = 0, ry = 0, lado = 0; ((Viewport3D*)n)->CandadoCamaraRect(rx, ry, lado);
            printf("@cap candado x=%d y=%d lado=%d\n", n->x + rx, n->y + ry, lado);
        }
        for (size_t i = 0; i < n->BarButtons.size(); i++) {
            Button* b = n->BarButtons[i];
            printf("@cap boton kind=%d i=%d rol=%d visible=%d x=%d y=%d w=%d h=%d texto=%s\n",
                   n->ViewportKind(), (int)i, b->rol, b->visible ? 1 : 0, b->sx, b->sy, b->width, b->height, b->text.c_str());
        }
        // la BARRA DE ABAJO (toolbar): Mover / Rotar / Escalar... (las ultimas acciones), la tilde y la cruz
        if (n->ToolbarVisible()) {
            n->ToolbarActualizar();
            for (size_t i = 0; i < n->ToolButtons.size(); i++) {
                Button* b = n->ToolButtons[i];
                if (!b->visible) continue;
                printf("@cap herramienta kind=%d i=%d rol=%d x=%d y=%d w=%d h=%d nombre=%s texto=%s\n",
                       n->ViewportKind(), (int)i, b->rol, b->sx, b->sy, b->width, b->height, b->ayuda.c_str(), b->text.c_str());
            }
        }
        // las MANIJAS del gizmo (coords de PANTALLA): 0..2 ejes X/Y/Z, 3..5 planos, 6 centro, 7 anillo de la vista, 8 libre
        if (n->ViewportKind() == 1) {
            for (int k = GizmoEjeX; k <= GizmoLibre; k++) {
                float sx = 0, sy = 0;
                if (GizmoManijaEnPantalla((Viewport3D*)n, k, sx, sy))
                    printf("@cap gizmo manija=%d x=%d y=%d modo=%d\n", k, n->x + (int)(sx + 0.5f), n->y + (int)(sy + 0.5f), GizmoModo());
            }
        }
        // los OBJETOS en la Vista 3D: donde cae el origen de cada uno en pantalla (para apuntarles: cajas, clicks)
        if (n->ViewportKind() == 1) InfoObjetos((Viewport3D*)n, SceneCollection);
        if (n->ViewportKind() == 1) {   // el cursor 3D, en pantalla
            float sx = 0, sy = 0;
            if (((Viewport3D*)n)->ProyectarPunto(cursor3D.pos, sx, sy))
                printf("@cap cursor3d x=%d y=%d\n", n->x + (int)(sx + 0.5f), n->y + (int)(sy + 0.5f));
        }
        // la LINEA DE TIEMPO: el panel de filas (Summary / objeto / canales), donde cae cada fila y cada keyframe (en el
        // dope sheet, sobre la fila; en el editor de curvas, sobre su curva) y la barra de numeros (para cambiar de
        // cuadro con un click). Para filmarla con el mouse: elegir un keyframe, una curva, borrar una fila
        if (n->ViewportKind() == 5) {
            Timeline* tl = (Timeline*)n;
            tl->ConstruirDopeRows();
            const bool curvas = (tl->modo == Timeline::TL_MODO_CURVAS);
            printf("@cap tlpanel x=%d y=%d w=%d ojo=%d curvas=%d numeros=%d f1=%d f11=%d\n", n->x, n->y + tl->stripY, tl->panelW,
                   n->x + tl->DopeOjoX(), curvas ? 1 : 0, n->y + tl->numY + tl->barH2 / 2,
                   n->x + (int)(tl->FrameToX(1.0f) + 0.5f), n->x + (int)(tl->FrameToX(11.0f) + 0.5f));
            const int rh = tl->rowH > 0 ? tl->rowH : 1;
            for (size_t r = 0; r < tl->dopeRows.size(); r++) {
                const Timeline::DopeRow& d = tl->dopeRows[r];
                const int ry = n->y + tl->stripY + tl->PosY + (int)r * rh + rh / 2;
                printf("@cap tlfila r=%d y=%d tipo=%d prop=%d comp=%d nombre=%s\n", (int)r, ry, d.tipo, d.propId, d.compId,
                       d.nombre.c_str());
                const AnimProperty* ap = (curvas && d.propId >= 0) ? tl->CurvaDeFila(d, false) : NULL;
                for (size_t k = 0; k < d.keys.size(); k++) {
                    const int f = d.keys[k];
                    const int ky = ap ? n->y + (int)(tl->ValueToY(ap->Eval(f, 0.0f)) + 0.5f) : ry;
                    printf("@cap tlkey r=%d f=%d x=%d y=%d\n", (int)r, f, n->x + (int)(tl->FrameToX((float)f) + 0.5f), ky);
                }
            }
        }
        // las FILAS del Outliner (la vista Escena): donde cae el nombre de cada objeto que se ve
        if (n->ViewportKind() == 2 && ((Outliner*)n)->vista == OUT_VISTA_ESCENA) InfoFilas((Outliner*)n, SceneCollection);
        // PROPIEDADES: las pestanias, las tarjetas (su titulo) y las filas, con el centro del valor y del rombo.
        // El MISMO recorrido que Properties::ClickEn (si no, un click del harness cae en otra fila).
        if (n->ViewportKind() == 3) {
            Properties* P = (Properties*)n;
            for (size_t i = 0; i < P->BarTabs.size(); i++) {
                Tab* t = P->BarTabs[i];
                if (t->visible) printf("@cap pestania i=%d x=%d y=%d w=%d h=%d activa=%d\n", (int)i, t->sx, t->sy, t->width, t->height, t->activa ? 1 : 0);
            }
            int yCursor = P->y + P->BarTopOffset() + P->PosY + borderGS + RenglonHeightGS + gapGS;
            for (size_t i = 0; i < P->GroupProperties.size(); i++) {
                GroupPropertie* g = P->GroupProperties[i];
                if (!g->visible) continue;
                const int hCab = borderGS + RenglonHeightGS + gapGS;
                const int x0 = P->x + P->PosX + borderGS;
                printf("@cap tarjeta i=%d x=%d y=%d w=%d h=%d abierta=%d titulo=%s\n", (int)i, x0, yCursor, g->width, hCab, g->open ? 1 : 0, g->name.c_str());
                if (g->open) {
                    int yFila = yCursor + hCab;
                    const int xDer = P->x + P->PosX + borderGS * 2 + g->width - bordersGS;
                    for (size_t j = 0; j < g->properties.size(); j++) {
                        PropertieBase* prop = g->properties[j];
                        const int hFila = prop->Resize(g->width);
                        if (hFila > 0) {
                            const int finValor = xDer - (g->keyBtnCol > 0 ? g->keyBtnCol : 0);
                            const int iniValor = x0 + g->colEtiqueta;
                            const int rx = (prop->AnimProp() >= 0 && g->keyBtnCol > 0) ? xDer - g->keyBtnCol / 2 : -1;
                            printf("@cap prop g=%d j=%d y=%d h=%d tipo=%d valor=%d rombo=%d nombre=%s\n", (int)i, (int)j, yFila + hFila / 2, hFila,
                                   (int)prop->GetType(), (iniValor + finValor) / 2, rx, prop->name.c_str());
                        }
                        yFila += hFila;
                    }
                }
                yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
            }
            // el OJO de cada script de la lista (tarjeta Control): donde tocar para prenderlo / apagarlo
            if (ObjActivo && ObjActivo->scriptDatos) {
                for (size_t f = 0; f < ObjActivo->scriptDatos->scripts.size(); f++) {
                    int ox, oy;
                    if (PropsOjoScriptCentro(P, (int)f, &ox, &oy))
                        printf("@cap ojoscript fila=%d x=%d y=%d apagado=%d\n", (int)f, ox, oy,
                               ObjActivo->scriptDatos->scripts[f].apagado ? 1 : 0);
                }
            }
        }
    }

    void InfoMenu(PopupMenu* m, int nivel) {
        if (!m || !m->abierto) return;
        printf("@cap menu nivel=%d x=%d y=%d w=%d h=%d filas=%d scroll=%d sel=%d titulo=%s\n",
               nivel, m->x, m->y, m->width, m->height, (int)m->items.size(), m->scroll, m->selectIndex, m->titulo.c_str());
        const int fila = RenglonHeightGS + gapGS;
        const int oy = m->titulo.empty() ? 0 : fila;
        const int visibles = m->FilasVisibles();
        for (int i = 0; i < (int)m->items.size(); i++) {
            const int v = i - m->scroll;
            if (v < 0 || v >= visibles) continue;
            const int cy = m->y + borderGS + oy + v * fila + RenglonHeightGS / 2;
            printf("@cap item nivel=%d i=%d id=%d cx=%d cy=%d texto=%s\n",
                   nivel, i, m->items[i]->id, m->x + m->width / 2, cy, m->items[i]->text.c_str());
        }
        if (m->submenuAbierto) InfoMenu(m->submenuAbierto, nivel + 1);
    }
}

// ============================================================================================================
//  CONTROLES de las pruebas de regresion de lo BASICO (tools/pruebas/prueba_basicos_tutoriales.w3s). El duenio,
//  2026-11-09: "sobre estos errores basicos... espero que se hagan pruebas automaticas para que nunca mas ocurran;
//  la gente tiene que poder hacer estos tutoriales en cualquier sistema y en todos conseguir el mismo resultado".
//  Cada error se prueba por la MISMA puerta que usa la persona (clicks, teclas, dedos), no por la funcion de adentro.
// ============================================================================================================
namespace {
    ViewportBase* HojaDeTipo(ViewportBase* n, int kind) {
        if (!n) return NULL;
        if (!n->isLeaf()) {
            ViewportBase* a = HojaDeTipo(((ViewportColumn*)n)->childA, kind);
            return a ? a : HojaDeTipo(((ViewportColumn*)n)->childB, kind);
        }
        return n->ViewportKind() == kind ? n : NULL;
    }
    void ClickEn(int x, int y, int clicks) {
        if (x != gRatonX || y != gRatonY) Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
        gRatonX = x; gRatonY = y;
        for (int k = 1; k <= clicks; k++) {
            gClicks = k;
            Boton(0, SDL_BUTTON_LEFT, true, x, y);
            Boton(0, SDL_BUTTON_LEFT, false, x, y);
        }
        gClicks = 1;
    }
    void TeclaEv(SDL_Keycode k, bool abajo, Uint16 mod) {
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = abajo ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.timestamp = SDL_GetTicks();
        e.key.state = abajo ? SDL_PRESSED : SDL_RELEASED;
        e.key.keysym.sym = k;
        e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
        e.key.keysym.mod = mod;
        Despachar(e);
    }
    // como una persona en un teclado: Shift apretado para las mayusculas, la tecla, la letra (TEXTINPUT) y soltar
    void Escribir(const std::string& s) {
        for (size_t i = 0; i < s.size(); i++) {
            const char ch = s[i];
            const bool mayus = (ch >= 'A' && ch <= 'Z');
            const char baja = mayus ? (char)(ch - 'A' + 'a') : ch;
            if (mayus) TeclaEv(SDLK_LSHIFT, true, KMOD_LSHIFT);
            TeclaEv((SDL_Keycode)baja, true, mayus ? KMOD_LSHIFT : 0);
            SDL_Event t; memset(&t, 0, sizeof(t)); t.type = SDL_TEXTINPUT; t.text.timestamp = SDL_GetTicks();
            t.text.text[0] = ch; Despachar(t);
            TeclaEv((SDL_Keycode)baja, false, mayus ? KMOD_LSHIFT : 0);
            if (mayus) TeclaEv(SDLK_LSHIFT, false, 0);
        }
    }
    std::string Resto(std::istringstream& ss) {
        std::string r; std::getline(ss, r);
        const size_t p = r.find_first_not_of(" \t");
        return p == std::string::npos ? std::string() : r.substr(p);
    }
    // la fila de Propiedades llamada 'nombre' (ya traducido): el centro de su valor. El MISMO recorrido que capinfo
    // y que Properties::ClickEn. false = no esta, o esta fuera de la pantalla
    bool FilaPropiedades(const std::string& nombre, int& x, int& y, PropertieBase** out) {
        Properties* P = (Properties*)HojaDeTipo(rootViewport, 3);
        if (!P) return false;
        int yCursor = P->y + P->BarTopOffset() + P->PosY + borderGS + RenglonHeightGS + gapGS;
        for (size_t i = 0; i < P->GroupProperties.size(); i++) {
            GroupPropertie* g = P->GroupProperties[i];
            if (!g->visible) continue;
            const int hCab = borderGS + RenglonHeightGS + gapGS;
            const int x0 = P->x + P->PosX + borderGS;
            if (g->open) {
                int yFila = yCursor + hCab;
                const int xDer = P->x + P->PosX + borderGS * 2 + g->width - bordersGS;
                for (size_t j = 0; j < g->properties.size(); j++) {
                    PropertieBase* prop = g->properties[j];
                    const int hFila = prop->Resize(g->width);
                    if (hFila > 0 && prop->name == nombre) {
                        const int finValor = xDer - (g->keyBtnCol > 0 ? g->keyBtnCol : 0);
                        x = (x0 + g->colEtiqueta + finValor) / 2; y = yFila + hFila / 2;
                        if (out) *out = prop;
                        return y >= P->y && y < P->y + P->height;
                    }
                    yFila += hFila;
                }
            }
            yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
        }
        return false;
    }
    bool FilaOutlinerDe(Outliner* o, Object* raiz, const std::string& nom, int& x, int& y) {
        if (!raiz) return false;
        for (size_t i = 0; i < raiz->Childrens.size(); i++) {
            Object* ob = raiz->Childrens[i];
            const int fila = OutlinerFilaDeObjeto(ob);
            if (fila >= 0 && ob->name == nom) {
                int prof = 0;
                for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
                x = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
                y = o->y + borderGS + o->PosY + o->BarTopOffset() + fila * (int)RenglonHeightGS + (int)RenglonHeightGS / 2;
                return true;
            }
            if (FilaOutlinerDe(o, ob, nom, x, y)) return true;
        }
        return false;
    }
    Mesh* MallaLlamada(const std::string& nom) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
        return (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
    }
}

bool W3dCapturasCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& manejado) {
    manejado = true;
    if (cmd == "raton") {
        std::string que, cual; int x = 0, y = 0; ss >> que >> x >> y >> cual;
        const int b = BotonDe(cual);
        if (que == "move") {
            Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
        } else if (que == "down" || que == "up") {
            if (x != gRatonX || y != gRatonY) Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
            const bool abajo = (que == "down");
            if (abajo) gRatonBotones |= SDL_BUTTON(b); else gRatonBotones &= ~SDL_BUTTON(b);
            Boton(0, b, abajo, x, y);
        } else if (que == "doble") {   // doble click: dos clicks seguidos, el segundo con clicks = 2
            if (x != gRatonX || y != gRatonY) Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
            for (int k = 1; k <= 2; k++) {
                gClicks = k;
                gRatonBotones |= SDL_BUTTON(b); Boton(0, b, true, x, y);
                gRatonBotones &= ~SDL_BUTTON(b); Boton(0, b, false, x, y);
            }
            gClicks = 1;
        } else { err = "raton: uso: raton down|move|up|doble <x> <y> [izq|medio|der]"; return false; }
        gRatonX = x; gRatonY = y;
        return true;
    }
    // 'ojoscript <fila>': un click de verdad en el OJO de ese script (la lista de la tarjeta Control, en Propiedades)
    // 'scriptapagado <fila> 0|1': el script de esa fila del objeto activo esta apagado (1) o prendido (0)
    // 'guardarcomofijar <nombre> <interno 0|1>': llena el "Guardar como..." abierto (el nombre y la casilla "Dentro del
    // .w3d") SIN guardar: para la captura del formulario (guardarcomo, en W3dScript.cpp, lo llena y guarda)
    if (cmd == "guardarcomofijar") {
        std::string nombre; int interno = -1; ss >> nombre >> interno;
        if (!GuardarComoAbierto()) { err = "guardarcomofijar: el formulario no esta abierto"; return false; }
        GuardarComoFijar(nombre, interno, std::string(), std::string());
        g_redraw = true; Cuadro();
        return true;
    }
    // 'scriptanadir': el boton Aniadir de la tarjeta Control (objeto activo): abre su menu (los scripts del proyecto y
    // "Cargar .lua..."); despues se elige con menuelegir <nombre>
    if (cmd == "scriptanadir") {
        if (!ObjActivo) { err = "scriptanadir: no hay objeto activo"; return false; }
        extern void _ScriptAgregarFwd(); _ScriptAgregarFwd();
        g_redraw = true; Cuadro();
        return true;
    }
    if (cmd == "ojoscript" || cmd == "scriptapagado") {
        int fila = -1, esperado = -1; ss >> fila >> esperado;
        if (!ObjActivo || !ObjActivo->scriptDatos || fila < 0 || fila >= (int)ObjActivo->scriptDatos->scripts.size()) {
            err = cmd + ": el objeto activo no tiene el script " + std::to_string(fila); return false;
        }
        if (cmd == "scriptapagado") {
            const int ap = ObjActivo->scriptDatos->scripts[fila].apagado ? 1 : 0;
            printf("      [scriptapagado] fila %d: %s\n", fila, ap ? "APAGADO" : "prendido");
            if (esperado >= 0 && ap != esperado) { err = "scriptapagado: se esperaba " + std::to_string(esperado); return false; }
            return true;
        }
        Properties* P = NULL;
        struct VK { static ViewportBase* Buscar(ViewportBase* n) { if (!n) return NULL; if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
                        ViewportBase* a = Buscar(((ViewportColumn*)n)->childA); if (a) return a; return Buscar(((ViewportColumn*)n)->childB); } };
        P = (Properties*)VK::Buscar(rootViewport);
        int x = 0, y = 0;
        if (!P || !PropsOjoScriptCentro(P, fila, &x, &y)) { err = "ojoscript: no se ve el ojo de la fila " + std::to_string(fila); return false; }
        Movio(0, gRatonBotones, x, y, gRatonX, gRatonY);
        gRatonBotones |= SDL_BUTTON(1); Boton(0, 1, true, x, y);
        gRatonBotones &= ~SDL_BUTTON(1); Boton(0, 1, false, x, y);
        gRatonX = x; gRatonY = y;
        return true;
    }
    // 'ayuda [texto en ingles]': el cartelito de ayuda (Button::ayuda, el que muestra W3dAyudaRender despues de un
    // momento quieto) de lo que esta bajo el mouse en Propiedades; con texto, falla si no es ese. (El cartelito en si no
    // sale en las capturas: el harness no dibuja la capa de los avisos)
    if (cmd == "ayuda") {
        std::string esperado; std::getline(ss, esperado);
        const size_t p0 = esperado.find_first_not_of(" \t"); esperado = (p0 == std::string::npos) ? "" : esperado.substr(p0);
        struct VK { static ViewportBase* Buscar(ViewportBase* n) { if (!n) return NULL; if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
                        ViewportBase* a = Buscar(((ViewportColumn*)n)->childA); if (a) return a; return Buscar(((ViewportColumn*)n)->childB); } };
        Properties* P = (Properties*)VK::Buscar(rootViewport);
        Button* b = P ? PropsAyudaEn(P, gRatonX, gRatonY) : NULL;
        const std::string hay = b ? b->ayuda : std::string();
        printf("      [ayuda] (%d, %d): '%s'\n", gRatonX, gRatonY, hay.c_str());
        if (!esperado.empty() && hay != esperado) { err = "ayuda: se esperaba '" + esperado + "'"; return false; }
        return true;
    }
    if (cmd == "dedo") {
        std::string que; int x = 0, y = 0, id = 1; ss >> que >> x >> y; if (!(ss >> id)) id = 1;
        // el orden de SDL: primero el mouse sintetizado (which = SDL_TOUCH_MOUSEID), despues el del dedo. SDL
        // sintetiza el mouse solo con el PRIMER dedo (id 1): los demas (dos dedos = zoom y paneo) son solo dedo
        const bool primero = (id == 1);
        std::map<int, std::pair<int, int> >::iterator ant = gDedos.find(id);
        const int px = (ant != gDedos.end()) ? ant->second.first : x;
        const int py = (ant != gDedos.end()) ? ant->second.second : y;
        if (que == "down") {
            if (primero) { Movio(SDL_TOUCH_MOUSEID, 0, x, y, x, y); Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y); }
            Dedo(SDL_FINGERDOWN, id, x, y, x, y);
        } else if (que == "move") {
            if (primero) Movio(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, x, y, px, py);
            Dedo(SDL_FINGERMOTION, id, x, y, px, py);
        } else if (que == "up") {
            if (primero) Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, x, y);
            Dedo(SDL_FINGERUP, id, x, y, px, py);
        } else { err = "dedo: uso: dedo down|move|up <x> <y> [id]"; return false; }
        if (que == "up") gDedos.erase(id); else gDedos[id] = std::make_pair(x, y);
        return true;
    }
    // las teclas modificadoras MANTENIDAS mientras se usa el mouse: 'mod shift', 'mod alt shift', 'mod' (ninguna).
    // El viewport 3D las lee del estado de SDL (Shift + boton del medio = desplazar; Alt + izquierdo = orbitar), que
    // los eventos de tecla sintetizados no tocan
    if (cmd == "mod") {
        int m = KMOD_NONE; bool shift = false, alt = false, ctrl = false; std::string t;
        while (ss >> t) {
            if (t == "shift") { m |= KMOD_LSHIFT; shift = true; }
            else if (t == "alt") { m |= KMOD_LALT; alt = true; }
            else if (t == "ctrl") { m |= KMOD_LCTRL; ctrl = true; }
            else { err = "mod: uso: mod [shift] [alt] [ctrl]  (sin nada: ninguna)"; return false; }
        }
        SDL_SetModState((SDL_Keymod)m);
        LShiftPressed = shift; LAltPressed = alt; LCtrlPressed = ctrl;   // (Ctrl + A mira LCtrlPressed)
        return true;
    }
    if (cmd == "rueda") {
        int pasos = 0, x = gRatonX, y = gRatonY; ss >> pasos >> x >> y;
        if (pasos == 0) { err = "rueda: uso: rueda <pasos> <x> <y> (positivo = arriba)"; return false; }
        Rueda(pasos, x, y);
        return true;
    }
    if (cmd == "tecla") {
        std::string nombre, como; ss >> nombre >> como;
        for (size_t i = 0; i < nombre.size(); i++) if (nombre[i] == '_') nombre[i] = ' ';   // "Keypad_7" = "Keypad 7"
        const SDL_Keycode k = SDL_GetKeyFromName(nombre.c_str());
        if (k == SDLK_UNKNOWN) { err = "tecla: SDL no conoce '" + nombre + "'"; return false; }
        for (int paso = 0; paso < 2; paso++) {
            const bool abajo = (paso == 0);
            if ((como == "down" && !abajo) || (como == "up" && abajo)) continue;
            SDL_Event e; memset(&e, 0, sizeof(e));
            e.type = abajo ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.timestamp = SDL_GetTicks();
            e.key.state = abajo ? SDL_PRESSED : SDL_RELEASED;
            e.key.keysym.sym = k;
            e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
            e.key.keysym.mod = (Uint16)SDL_GetModState();   // los que dejo 'mod' (Ctrl + A = 'mod ctrl' y 'tecla a')
            Despachar(e);
        }
        return true;
    }
    if (cmd == "cuadro") { Cuadro(); return true; }
    // 'modo <objeto|edicion|pose|vertices|pesos|textura>': el modo del objeto activo, por la MISMA puerta que el menu
    // del boton de modo (LayoutModoElegir). El 'mode' de las pruebas no tiene el Modo Pose.
    if (cmd == "modo") {
        std::string q; ss >> q;
        const int m = (q == "objeto") ? ObjectMode : (q == "edicion") ? EditMode : (q == "pose") ? PoseMode :
                      (q == "vertices") ? VertexPaint : (q == "pesos") ? WeightPaint : (q == "textura") ? TexturePaint : -1;
        if (m < 0) { err = "modo: objeto|edicion|pose|vertices|pesos|textura"; return false; }
        extern void LayoutModoElegir(int modo); LayoutModoElegir(m);
        if (InteractionMode != m) { err = "modo: el objeto activo no tiene ese modo"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'outliner n95 mover|visible|render': las teclas 1 / 2 / 3 del N95 con el Outliner activo (las MISMAS llamadas que
    // platform/symbian/src/w3dlayout.cpp: W3dOutlinerMoverToggle / ToggleVisible / ToggleRender).
    // 'outliner tecla <Up|Down|Left|Right|Enter|Cancel>': una tecla del N95 al panel activo (LayoutTeclaPanelActivo).
    if (cmd == "outliner") {
        std::string que, cual; ss >> que >> cual;
        if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 2) { err = "outliner: el viewport activo no es un Outliner"; return false; }
        Outliner* out = (Outliner*)viewPortActive;
        if (que == "n95") {
            if (cual == "mover") {
                if (out->ModoMover()) out->MoverConfirmar();
                else if (out->moviendoRec) out->MoverRecConfirmar();
                else out->MoverIniciar();
            } else if (cual == "visible") { extern void ChangeVisibilityObj(); ChangeVisibilityObj(); }
            else if (cual == "render") { extern void ChangeRenderizableObj(); ChangeRenderizableObj(); }
            else { err = "outliner n95: mover|visible|render"; return false; }
        } else if (que == "tecla") {   // (LayoutKey, LayoutInput.h: los codigos propios del panel, no los de SDL)
            const int k = (cual == "Up") ? 0 : (cual == "Down") ? 1 : (cual == "Left") ? 2 : (cual == "Right") ? 3 :
                          (cual == "Enter") ? 4 : (cual == "Cancel") ? 5 : -1;
            if (k < 0) { err = "outliner tecla: Up|Down|Left|Right|Enter|Cancel"; return false; }
            { extern bool LayoutTeclaPanelActivo(int tecla); LayoutTeclaPanelActivo(k); }
        } else { err = "outliner: uso: outliner n95 <mover|visible|render> | outliner tecla <tecla>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'tick': el reloj de los controles (ControlesTick: las pulsaciones largas tactiles), como en cada cuadro del editor
    if (cmd == "tick") { extern void ControlesTick(); ControlesTick(); g_redraw = true; Cuadro(); return true; }
    // 'menuabierto si|no': que haya (o no) un menu desplegado ahora (la pulsacion larga abre el contextual)
    if (cmd == "menuabierto") {
        std::string q; ss >> q;
        extern PopupMenu* MenuAbierto;
        const bool hay = MenuAbierto && MenuAbierto->abierto;
        printf("      [menuabierto] %s%s%s\n", hay ? "si" : "no", hay ? ": " : "", hay ? MenuAbierto->titulo.c_str() : "");
        if ((q == "si") != hay) { err = std::string("menuabierto: esperaba ") + q; return false; }
        return true;
    }
    // 'texto <caracteres>': lo tipeado (SDL_TEXTINPUT, como el teclado de verdad despues de la tecla): los numeros de
    // un transform ("R Z 90"), un nombre... Las teclas sueltas ('tecla') no escriben: solo mandan la tecla.
    if (cmd == "texto") {
        std::string t; ss >> t;
        if (t.empty()) { err = "texto: uso: texto <caracteres>"; return false; }
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_TEXTINPUT;
        e.text.timestamp = SDL_GetTicks();
        strncpy(e.text.text, t.c_str(), sizeof(e.text.text) - 1);
        Despachar(e);
        return true;
    }
    // 'origen cursor|geometria': Objeto > Definir Origen > Origen al Cursor 3D / Origen a Geometria, sin pasar por el
    // menu (para preparar una escena: en el N95 la barra de menus no entra entera en la pantalla)
    if (cmd == "origen") {
        std::string que; ss >> que;
        extern void SetOriginToCursor(); extern void SetOriginOriginToGeometry();
        if (que == "cursor") SetOriginToCursor();
        else if (que == "geometria") SetOriginOriginToGeometry();
        else { err = "origen: uso: origen cursor|geometria"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'pivote <0..3>': el punto de pivote (TransformPivot: 0 Cursor 3D, 1 Origenes individuales, 2 Punto medio,
    // 3 Elemento activo). El boton de la barra de la Vista 3D muestra su icono.
    if (cmd == "pivote") {
        int v = -1; ss >> v;
        if (v < 0 || v > 3) { err = "pivote: uso: pivote <0..3>"; return false; }
        g_transformPivot = v;
        g_redraw = true; Cuadro();
        return true;
    }
    // 'cursor3d <x> <y> <z>': lleva el cursor 3D a ese punto (como el iman, sin pasar por el menu)
    if (cmd == "cursor3d") {
        float x = 0, y = 0, z = 0;
        if (!(ss >> x >> y >> z)) { err = "cursor3d: uso: cursor3d <x> <y> <z>"; return false; }
        cursor3D.pos = Vector3(x, y, z);
        g_redraw = true; Cuadro();
        return true;
    }
    // 'agregar sonido': Anadir > Sonido (en el cursor 3D, elegido). 'agregar restriccion <0..3>': la restriccion de ese
    // tipo (W3dConstraintTipo) en el objeto activo, por la puerta del boton Anadir de Propiedades. 'agregar
    // modificador <n>': el modificador de ese ModifierType en la malla activa (tambien Esqueleto, que 'modadd' no tiene).
    if (cmd == "agregar") {
        std::string que; ss >> que;
        if (que == "sonido") { extern void AddSonido(); AddSonido(); }
        else if (que == "vacio") { extern void AddEmpty(); AddEmpty(); }   // Aniadir > Vacio (en el cursor 3D)
        else if (que == "restriccion" || que == "modificador") {
            int t = -1; ss >> t;
            if (!ObjActivo || t < 0) { err = "agregar: sin objeto activo o sin tipo"; return false; }
            if (que == "restriccion") UndoConAgregar(ObjActivo, t);
            else {
                if (ObjActivo->getType() != ObjectType::mesh) { err = "agregar modificador: el activo no es una malla"; return false; }
                UndoModAgregar((Mesh*)ObjActivo, t);
                ((Mesh*)ObjActivo)->GenerarMallaModificada();
            }
        } else { err = "agregar: uso: agregar sonido | agregar restriccion <tipo> | agregar modificador <tipo>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'vista3d sombreado <n>': el modo de la Vista 3D activa (RenderType: 0 Solido, 1 Vista previa de material,
    // 2 Renderizado...). 'vista3d huesos <0|1>': dibujar o no los esqueletos (Superposiciones > Objetos).
    // 'vista3d gizmo <0|1>': Superposiciones > Gizmo (el N95 arranca sin gizmo). 'vista3d barra <0|1>': la barra de
    // abajo (en el N95 viene apagada: cfg.nuevoUsuario).
    if (cmd == "vista3d") {
        std::string que; int v = -1; ss >> que >> v;
        if (!Viewport3DActive && rootViewport) rootViewport->Render();
        Viewport3D* vp = Viewport3DActive;
        if (!vp) { err = "vista3d: no hay viewport 3D"; return false; }
        if (que == "sombreado" && v >= 0 && v <= (int)RenderType::Vectores) vp->view.v = (RenderType::Enum)v;
        else if (que == "huesos" && (v == 0 || v == 1)) vp->showArmature = (v == 1);
        else if (que == "gizmo" && (v == 0 || v == 1)) g_gizmoOn = (v == 1);   // (en el N95 arranca apagado)
        else if (que == "barra" && (v == 0 || v == 1)) { extern bool g_capturaSinBarra3D; g_capturaSinBarra3D = (v == 0); }
        else if (que == "overlays" && (v == 0 || v == 1)) vp->showOverlays = (v == 1);   // Superposiciones > Mostrar
        else { err = "vista3d: uso: vista3d sombreado <n> | vista3d huesos <0|1> | vista3d gizmo <0|1> | vista3d barra <0|1> | vista3d overlays <0|1>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'texturas': carga YA las texturas que el import dejo en la cola (el editor carga una por cuadro: en una foto
    // sacada enseguida, un modelo recien importado se veria sin textura)
    if (cmd == "texturas") { CargarTodasTexturasPendientes(); g_redraw = true; Cuadro(); return true; }
    // la escala de la interfaz, como la de Ajustes (1 = como se ve en el N95)
    if (cmd == "escala") {
        int v = 0; ss >> v;
        if (v < 1 || v > 6) { err = "escala: uso: escala <1..6>"; return false; }
        SetGlobalScale(v);
        if (rootViewport) rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        Cuadro();
        return true;
    }
    // el layout por defecto PARA EL TAMANIO ACTUAL, como si el editor hubiera arrancado asi (el harness abre con
    // otro tamanio y 'ventana' solo lo redimensiona): en 320x240 son 2 viewports, la Vista 3D y Propiedades, como
    // en el N95. El arbol viejo no se borra (algun puntero del input podria seguir mirandolo; el harness dura poco).
    if (cmd == "layoutpordefecto") {
        Viewport3D* vp3d = NULL;
        rootViewport = LayoutPorDefecto(MenuPantallaW, MenuPantallaH, &vp3d);
        viewPortActive = vp3d;
        Viewport3DActive = vp3d;
        rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        g_redraw = true;
        Cuadro();
        return true;
    }
    // la division de la raiz del layout (0..1, lo que ocupa el de la izquierda / el de arriba). En el N95 las
    // demos van con 2 viewports: la Vista 3D al 55 % y Propiedades al 45 %.
    if (cmd == "division") {
        float f = 0; ss >> f;
        if (!rootViewport || rootViewport->isLeaf()) { err = "division: la raiz del layout no esta dividida"; return false; }
        if (f < 0.05f || f > 0.95f) { err = "division: uso: division <0.05..0.95>"; return false; }
        if (rootViewport->ContainerKind() == 1) ((ViewportRow*)rootViewport)->splitFrac = f;
        else                                    ((ViewportColumn*)rootViewport)->splitFrac = f;
        rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        Cuadro();
        return true;
    }
    // 'divisionde <kind> <0.05..0.95>': la division que contiene a la hoja de ese tipo (5 = la Linea de tiempo): cuanto
    // ocupa su PRIMER hijo. Para que la Linea de tiempo se vea grande en los tutoriales de animacion
    if (cmd == "divisionde") {
        int k = -1; float f = 0; ss >> k >> f;
        if (f < 0.05f || f > 0.95f) { err = "divisionde: uso: divisionde <kind> <0.05..0.95>"; return false; }
        struct B {
            static ViewportBase* Padre(ViewportBase* n, int kind) {
                if (!n || n->isLeaf()) return NULL;
                ViewportColumn* c = (ViewportColumn*)n;
                if ((c->childA && c->childA->isLeaf() && c->childA->ViewportKind() == kind) ||
                    (c->childB && c->childB->isLeaf() && c->childB->ViewportKind() == kind)) return n;
                ViewportBase* r = Padre(c->childA, kind);
                return r ? r : Padre(c->childB, kind);
            }
        };
        ViewportBase* p = B::Padre(rootViewport, k);
        if (!p) { err = "divisionde: no hay una division con una hoja de ese tipo"; return false; }
        if (p->ContainerKind() == 1) ((ViewportRow*)p)->splitFrac = f;
        else                         ((ViewportColumn*)p)->splitFrac = f;
        rootViewport->Resize(MenuPantallaW, MenuPantallaH);
        Cuadro();
        return true;
    }
    // la tecla VERDE del N95: 'verde' = un toque (pasa al viewport siguiente); 'verde <dx> <dy> [cuadros]' = verde
    // mantenida + una flecha durante esos cuadros (el telefono redimensiona un paso de 0.01 por cuadro)
    if (cmd == "verde") {
        int dx = 0, dy = 0, cuadros = 1;
        if (!(ss >> dx >> dy)) { LayoutCiclarViewportActivo(1); g_redraw = true; Cuadro(); return true; }
        if (!(ss >> cuadros)) cuadros = 1;
        for (int i = 0; i < cuadros; i++) { LayoutRedimensionarViewportActivo(dx, dy, 0.01f); g_redraw = true; Cuadro(); }
        return true;
    }
    // el KEYPAD del N95 (platform/symbian/src/Whisk3DContainer.cpp + w3dnewscene.cpp): las MISMAS llamadas al
    // viewport 3D activo que hace el telefono. Lo que se mantiene apretado se aplica una vez por cuadro, como alla:
    //   n95 orbitar <dx> <dy> [cuadros]   las flechas mantenidas (8 por cuadro)
    //   n95 zoom <dy> [cuadros]           0 + arriba / abajo (dy -1 = arriba = acercar)
    //   n95 panear <dx> <dy> [cuadros]    * + las flechas
    //   n95 vista <dx> <dy>               # + una flecha (una vista por pulsacion)
    //   n95 perspectiva                   # solo, sin flecha
    //   n95 encuadrar                     0 solo, sin flecha
    //   n95 camara                        8 (fuera del Modo Edicion): mirar por la camara activa / volver
    //   n95 bloquearorbita                9: bloquear la orbita (lo que orbitaba, desplaza) / desbloquearla
    if (cmd == "n95") {
        std::string que; ss >> que;
        if (!Viewport3DActive && rootViewport) rootViewport->Render();   // (lo puebla el dibujado)
        Viewport3D* vp = Viewport3DActive;
        if (!vp) { err = "n95: no hay viewport 3D"; return false; }
        int a = 0, b = 0, cuadros = 1;
        if (que == "orbitar" || que == "panear") {
            if (!(ss >> a >> b)) { err = "n95: uso: n95 " + que + " <dx> <dy> [cuadros]"; return false; }
            if (!(ss >> cuadros)) cuadros = 1;
            for (int i = 0; i < cuadros; i++) {
                if (que == "orbitar") vp->OrbitarFlecha(a * 8, b * 8);
                else                  vp->PanFlecha(-a * 8, -b * 8);
                g_redraw = true; Cuadro();
            }
        } else if (que == "zoom") {
            if (!(ss >> b)) { err = "n95: uso: n95 zoom <dy> [cuadros]"; return false; }
            if (!(ss >> cuadros)) cuadros = 1;
            for (int i = 0; i < cuadros; i++) { vp->Zoom((float)(-b)); g_redraw = true; Cuadro(); }
        } else if (que == "vista") {
            if (!(ss >> a >> b)) { err = "n95: uso: n95 vista <dx> <dy>"; return false; }
            vp->VistaCuadranteNav(a, b);
        } else if (que == "perspectiva") vp->ChangePerspective();
        else if (que == "encuadrar") vp->EnfocarObject();
        else if (que == "camara") vp->SetViewFromCameraActive(!vp->ViewFromCameraActive);
        else if (que == "bloquearorbita") { extern bool LayoutLockOrbitToggle(); LayoutLockOrbitToggle(); }
        // MOVER / ROTAR / ESCALAR con el keypad (platform/symbian/src/w3dnewscene.cpp, W3dNewTransform*), en Modo Objeto:
        //   n95 transformar <1|2|3>              1 / 2 / 3 sin transform en curso: mover / rotar / escalar
        //   n95 eje <0|1|2>                      1 / 2 / 3 durante el transform: eje X / Y / Z (otra vez: cicla la orientacion)
        //   n95 transflechas <dx> <dy> [cuadros] las flechas mantenidas (mover: 4 por cuadro; rotar y escalar: 8)
        //   n95 aceptar | n95 cancelar           OK (el centro del pad) / C
        else if (que == "transformar") {
            int m = 0; ss >> m;
            if (m < 1 || m > 3) { err = "n95: uso: n95 transformar <1|2|3>"; return false; }
            // MODO EDICION: como W3dNewTransformStart del telefono (platform/symbian/src/w3dnewscene.cpp): el transform
            // es de los vertices, aristas o caras elegidos (el EditXform compartido), no del objeto entero
            if (InteractionMode == EditMode && g_editMesh) {
                extern void EditXformIniciar(); extern bool EditXformActivo();
                if (m == 1)      { estado = translacion; axisSelect = ViewAxis; }
                else if (m == 2) { estado = rotacion;    axisSelect = ViewAxis; }
                else             { estado = EditScale;   axisSelect = XYZ; }
                EditXformIniciar();
                if (!EditXformActivo()) { estado = editNavegacion; err = "n95 transformar: nada elegido en el Modo Edicion"; return false; }
                gModoN95 = m;
                g_redraw = true; Cuadro();
                return true;
            }
            if (!ObjActivo || !ObjActivo->select) { err = "n95 transformar: no hay un objeto activo seleccionado"; return false; }
            if (m == 1) SetPosicion();
            else {
                if (!guardarEstado()) { err = "n95 transformar: no se pudo guardar el estado"; return false; }
                estado = (m == 2) ? rotacion : EditScale;
                axisSelect = ViewAxis;
            }
            gModoN95 = m;
        } else if (que == "extruir") {   // la 7 del N95 en el Modo Edicion: extruye y queda moviendo (el lapiz + el numero)
            extern void LayoutExtrudeFaces(); LayoutExtrudeFaces();
            if (estado == editNavegacion) { err = "n95 extruir: no arranco (hay caras elegidas en el Modo Edicion?)"; return false; }
            gModoN95 = 1;
        } else if (que == "eje") {
            int e = 0; ss >> e;
            if (!gModoN95) { err = "n95 eje: no hay un transform en curso"; return false; }
            CiclarEjeTransform(e);
        } else if (que == "transflechas") {
            if (!gModoN95) { err = "n95 transflechas: no hay un transform en curso"; return false; }
            if (!(ss >> a >> b)) { err = "n95: uso: n95 transflechas <dx> <dy> [cuadros]"; return false; }
            if (!(ss >> cuadros)) cuadros = 1;
            const int d = (gModoN95 == 1) ? 4 : 8;
            for (int i = 0; i < cuadros; i++) {
                if (gModoN95 == 1) SetTranslacionObjetos(a * d, b * d, vp->VelocidadArrastreMundo());
                else if (gModoN95 == 2) SetRotacion(a * d, b * d);
                else SetScale(a * d, b * d, 0.001f);
                g_redraw = true; Cuadro();
            }
        } else if (que == "aceptar" || que == "cancelar") {
            if (!gModoN95) { err = "n95 " + que + ": no hay un transform en curso"; return false; }
            if (que == "aceptar") vp->Aceptar(); else { Cancelar(); estado = editNavegacion; }
            gModoN95 = 0;
        }
        else { err = "n95: accion desconocida '" + que + "' (orbitar|zoom|panear|vista|perspectiva|encuadrar|camara|bloquearorbita|transformar|eje|transflechas|aceptar|cancelar)"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'caras <n> <n> ...': en Modo Edicion, ELEGIR esas caras (solo esas, todas juntas). El 'select face' de los
    // tests deja una sola: cada una reemplaza a la anterior
    if (cmd == "caras") {
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "caras: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e) { err = "caras: la malla no tiene edit mesh"; return false; }
        EditSelectMode = SelFace;
        e->SeleccionarTodo(false);
        int f = 0, n = 0;
        while (ss >> f) {
            if (f < 0 || f >= e->NumFaces()) { err = "caras: cara fuera de rango"; return false; }
            e->TogglearFace(f, false);
            n++;
        }
        if (!n) { err = "caras: uso: caras <n> <n> ..."; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'stl exportar <ruta> [sel]': ExportSTL (todas las mallas o las seleccionadas), igual que Propiedades > Exportar.
    // 'stl revisar <malla> <puntos> <triangulos> <ymin> <ymax>': la malla (por nombre) tiene esos puntos UNICOS (los
    // vertices soldados), esos triangulos y esa altura en Y (en local): asi se ve que el STL entro unido y parado.
    // 'stl archivo <ruta> <triangulos> <zmax>': el .stl tiene esos triangulos y su Z mas alta es esa (Z arriba).
    if (cmd == "stl") {
        std::string que; ss >> que;
        if (que == "exportar") {
            std::string ruta, sel; ss >> ruta >> sel;
            if (!ExportSTL(ruta, sel == "sel", true, true)) { err = "stl exportar: fallo"; return false; }
            return true;
        }
        if (que == "revisar") {
            std::string nom; int puntos = -1, tris = -1; float ymin = 0, ymax = 0; ss >> nom >> puntos >> tris >> ymin >> ymax;
            Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
            if (!o || o->getType() != ObjectType::mesh) { err = "stl revisar: no hay una malla '" + nom + "'"; return false; }
            Mesh* m = (Mesh*)o;
            std::set<std::string> unicos;
            float y0 = 1e30f, y1 = -1e30f;
            for (int i = 0; i < m->vertexSize; i++) {
                unicos.insert(std::string((const char*)&m->vertex[i * 3], 12));
                if (m->vertex[i * 3 + 1] < y0) y0 = m->vertex[i * 3 + 1];
                if (m->vertex[i * 3 + 1] > y1) y1 = m->vertex[i * 3 + 1];
            }
            char b[200];
            snprintf(b, sizeof b, "stl revisar: '%s' tiene %d puntos, %d triangulos, Y de %.3f a %.3f (esperaba %d, %d, %.3f a %.3f)",
                     nom.c_str(), (int)unicos.size(), m->facesSize / 3, y0, y1, puntos, tris, ymin, ymax);
            printf("      %s\n", b);
            if ((int)unicos.size() != puntos || m->facesSize / 3 != tris || fabsf(y0 - ymin) > 1e-3f || fabsf(y1 - ymax) > 1e-3f) {
                err = b; return false;
            }
            return true;
        }
        if (que == "archivo") {
            std::string ruta; int tris = -1; float zmax = 0; ss >> ruta >> tris >> zmax;
            std::vector<unsigned char> d;
            if (!w3dFileSystem::ReadFileBytes(ruta, d) || d.size() < 84) { err = "stl archivo: no pude leer " + ruta; return false; }
            const unsigned int n = (unsigned int)d[80] | ((unsigned int)d[81] << 8) | ((unsigned int)d[82] << 16) | ((unsigned int)d[83] << 24);
            if (d.size() != 84 + (size_t)n * 50) { err = "stl archivo: el largo no cuadra con la cantidad de triangulos"; return false; }
            float z1 = -1e30f;
            for (unsigned int t = 0; t < n; t++)
                for (int v = 0; v < 3; v++) {
                    float z; memcpy(&z, &d[84 + (size_t)t * 50 + 12 + 12 * v + 8], 4);
                    if (z > z1) z1 = z;
                }
            char b[160];
            snprintf(b, sizeof b, "stl archivo: %u triangulos, Z max %.3f (esperaba %d, %.3f)", n, z1, tris, zmax);
            printf("      %s\n", b);
            if ((int)n != tris || fabsf(z1 - zmax) > 1e-3f) { err = b; return false; }
            return true;
        }
        err = "stl: uso: stl exportar|revisar|archivo ...";
        return false;
    }
    // 'cobertura': que la FUENTE de la interfaz pueda dibujar cada caracter de cada traduccion (res/idiomas/*.txt).
    // Usa la fuente de verdad (Font::Soporta: font.png + font-ext.png + las compuestas), no una lista aparte.
    if (cmd == "cobertura") {
        extern Font* WhiskFont;
        if (!WhiskFont) { err = "cobertura: no hay fuente"; return false; }
        int faltan = 0;
        for (int idi = 1; idi < (int)W3dLangCantidad; idi++) {
            std::vector<unsigned char> b;
            const std::string ruta = w3dFileSystem::GetResDir() + "/idiomas/" + W3dIdiomaCodigo((W3dIdioma)idi) + ".txt";
            if (!w3dFileSystem::ReadFileBytes(ruta, b) || b.empty()) { printf("@cap cobertura %s: sin archivo\n", W3dIdiomaCodigo((W3dIdioma)idi)); continue; }
            b.push_back(0);
            std::set<unsigned> malos;
            size_t i = 0;
            while (b[i]) {
                const uint16_t cp = UTF8_Char((const char*)&b[0], i);
                if (cp == '\n' || cp == '\t' || cp == '\r') continue;
                if (!WhiskFont->Soporta(cp)) malos.insert(cp);
            }
            printf("@cap cobertura %s: %d caracter(es) sin dibujo", W3dIdiomaCodigo((W3dIdioma)idi), (int)malos.size());
            for (std::set<unsigned>::iterator it = malos.begin(); it != malos.end(); ++it) printf(" U+%04X", *it);
            printf("\n");
            faltan += (int)malos.size();
        }
        if (faltan) { err = "cobertura: la fuente no tiene todos los caracteres de las traducciones"; return false; }
        return true;
    }
    if (cmd == "capinfo") {
        if (!rootViewport) { err = "capinfo: no hay layout de viewports"; return false; }
        printf("@cap pantalla w=%d h=%d\n", MenuPantallaW, MenuPantallaH);
        InfoHojas(rootViewport);
        InfoMenu(MenuAbierto, 0);
        // la VENTANA EMERGENTE abierta (un "Guardar como...", el explorador...): su rectangulo (para recortar una captura)
        if (PopUpActive && PopUpActive->popUpWindow)
            printf("@cap popup x=%d y=%d w=%d h=%d nombre=%s\n", PopUpActive->x, PopUpActive->y, (int)PopUpActive->popUpWindow->width,
                   (int)PopUpActive->popUpWindow->height, PopUpActive->name.c_str());
        // el EXPLORADOR DE ARCHIVOS, si esta abierto: cada archivo / carpeta de la grilla (su rectangulo en la pantalla y
        // su nombre) y los botones de abajo (la accion, como "Abrir", y Cancelar)
        if (FileBrowser* fb = dynamic_cast<FileBrowser*>(PopUpActive)) {
            printf("@cap explorador ruta=%s\n", fb->currentPath.c_str());
            const int cols = fb->cols > 0 ? fb->cols : 1;
            for (size_t k = 0; k < fb->entries.size(); k++) {
                const int ex = fb->fileX + (int)(k % cols) * fb->cellW, ey = fb->fileY + (int)(k / cols) * fb->cellH;
                printf("@cap archivo i=%d x=%d y=%d w=%d h=%d sel=%d nombre=%s\n", (int)k, ex, ey, fb->cellW, fb->cellH,
                       fb->selected == (int)k ? 1 : 0, fb->entries[k].name.c_str());
            }
            Button* bs[2] = { fb->btnAction, fb->btnCancel };
            for (int k = 0; k < 2; k++)
                if (bs[k]) printf("@cap exploradorboton x=%d y=%d w=%d h=%d texto=%s\n", bs[k]->sx, bs[k]->sy, bs[k]->width,
                                  bs[k]->height, bs[k]->text.c_str());
        }
        // el TECLADO NUMERICO tactil, si esta abierto: el centro de cada tecla
        {
            static const char* kTec[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "*", "/", "+", "-", "(", ")", "DEL", "<", ">", "X", "OK" };
            for (size_t k = 0; k < sizeof(kTec) / sizeof(kTec[0]); k++) {
                int tx = 0, ty = 0;
                if (NumPadTeclaCentro(kTec[k], tx, ty)) printf("@cap numpad x=%d y=%d texto=%s\n", tx, ty, kTec[k]);
            }
        }
        // el RELOJ de la Linea de tiempo, si esta abierto: el centro de cada campo (Cuadro actual / Inicio / Fin / FPS)
        {
            extern bool TiempoPopupCampo(int j, int& cx, int& cy, std::string& nombre);
            int cx = 0, cy = 0; std::string nom;
            for (int j = 0; j < 4 && TiempoPopupCampo(j, cx, cy, nom); j++)
                printf("@cap tiempo j=%d x=%d y=%d nombre=%s\n", j, cx, cy, nom.c_str());
        }
        return true;
    }
    // ---- CONTROLES (ver arriba): las pruebas de lo basico ----
    // 'topologia <malla> <puntos> <aristas> <caras> [sueltas]': posiciones distintas, aristas, caras y lineas sueltas.
    // Asi se ve que una operacion no dejo restos (Separar) ni le falto una cara (Extruir un plano)
    if (cmd == "topologia") {
        std::string nom; int p = -1, a = -1, c = -1, su = -1; ss >> nom >> p >> a >> c; if (!(ss >> su)) su = -1;
        Mesh* m = MallaLlamada(nom);
        if (!m) { err = "topologia: no hay una malla '" + nom + "'"; return false; }
        std::set<std::string> u;
        for (int i = 0; i < m->vertexSize; i++) u.insert(std::string((const char*)&m->vertex[i * 3], 12));
        const int np = (int)u.size(), na = (int)(m->edges.size() / 2), nc = (int)m->faces3d.size(), ns = (int)(m->looseEdges.size() / 2);
        char b[240];
        snprintf(b, sizeof b, "topologia '%s': %d puntos, %d aristas, %d caras, %d sueltas (esperaba %d, %d, %d, %d)", nom.c_str(), np, na, nc, ns, p, a, c, su);
        printf("      %s\n", b);
        if (np != p || na != a || nc != c || (su >= 0 && ns != su)) { err = b; return false; }
        return true;
    }
    // 'volumen <malla> <min> <max>': el volumen con signo (en local) de una malla cerrada. Positivo = todas las caras
    // miran para AFUERA; una cara dada vuelta o que falta lo cambia
    if (cmd == "volumen") {
        std::string nom; float mn = 0, mx = 0; ss >> nom >> mn >> mx;
        Mesh* m = MallaLlamada(nom);
        if (!m) { err = "volumen: no hay una malla '" + nom + "'"; return false; }
        double v = 0;
        for (size_t f = 0; f < m->faces3d.size(); f++) {
            const std::vector<int>& ix = m->faces3d[f].idx;
            for (size_t k = 1; k + 1 < ix.size(); k++) {
                const GLfloat* a = &m->vertex[ix[0] * 3]; const GLfloat* b = &m->vertex[ix[k] * 3]; const GLfloat* c = &m->vertex[ix[k + 1] * 3];
                v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0;
            }
        }
        printf("      [volumen] '%s' = %.4f (esperaba %.4f a %.4f)\n", nom.c_str(), v, mn, mx);
        if (v < mn || v > mx) { char b[160]; snprintf(b, sizeof b, "volumen '%s': %.4f, fuera de %.4f a %.4f", nom.c_str(), v, mn, mx); err = b; return false; }
        return true;
    }
    // 'cajamundo <malla> <xmin> <ymin> <zmin> <xmax> <ymax> <zmax> <tol>': donde esta la malla EN EL MUNDO (interno, Y
    // arriba). Lo que no se tiene que mover (Origen al Cursor 3D mueve el origen, no la geometria)
    if (cmd == "cajamundo") {
        std::string nom; float e[6] = { 0, 0, 0, 0, 0, 0 }, tol = 0.001f; ss >> nom >> e[0] >> e[1] >> e[2] >> e[3] >> e[4] >> e[5] >> tol;
        Mesh* m = MallaLlamada(nom);
        if (!m || m->vertexSize <= 0) { err = "cajamundo: no hay una malla '" + nom + "'"; return false; }
        Matrix4 W; m->GetWorldMatrixBase(W);
        float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
        for (int i = 0; i < m->vertexSize; i++) {
            const GLfloat* q = &m->vertex[i * 3];
            const float w[3] = { W.m[0] * q[0] + W.m[4] * q[1] + W.m[8] * q[2] + W.m[12],
                                 W.m[1] * q[0] + W.m[5] * q[1] + W.m[9] * q[2] + W.m[13],
                                 W.m[2] * q[0] + W.m[6] * q[1] + W.m[10] * q[2] + W.m[14] };
            for (int k = 0; k < 3; k++) { if (w[k] < mn[k]) mn[k] = w[k]; if (w[k] > mx[k]) mx[k] = w[k]; }
        }
        char b[240];
        snprintf(b, sizeof b, "cajamundo '%s': %.3f %.3f %.3f a %.3f %.3f %.3f (esperaba %.3f %.3f %.3f a %.3f %.3f %.3f)", nom.c_str(),
                 mn[0], mn[1], mn[2], mx[0], mx[1], mx[2], e[0], e[1], e[2], e[3], e[4], e[5]);
        printf("      %s\n", b);
        for (int k = 0; k < 3; k++) if (fabsf(mn[k] - e[k]) > tol || fabsf(mx[k] - e[k + 3]) > tol) { err = b; return false; }
        return true;
    }
    // 'activo <nombre>': el objeto activo es ese
    if (cmd == "activo") {
        const std::string nom = Resto(ss);
        const std::string hay = ObjActivo ? ObjActivo->name : std::string("(ninguno)");
        printf("      [activo] %s (esperaba %s)\n", hay.c_str(), nom.c_str());
        if (hay != nom) { err = "activo: es '" + hay + "', se esperaba '" + nom + "'"; return false; }
        return true;
    }
    // 'ratonvista3d': el mouse al centro de la Vista 3D (un movimiento de verdad: el foco va con el mouse)
    if (cmd == "ratonvista3d") {
        ViewportBase* v = HojaDeTipo(rootViewport, 1);
        if (!v) { err = "ratonvista3d: no hay una Vista 3D"; return false; }
        const int x = v->x + v->width / 2, y = v->y + v->height / 2;
        Movio(0, gRatonBotones, x, y, gRatonX, gRatonY); gRatonX = x; gRatonY = y;
        return true;
    }
    // 'menuitem <texto en ingles>': el menu abierto tiene esa opcion (traducida al idioma del editor)
    if (cmd == "menuitem") {
        const std::string t = T(Resto(ss).c_str());
        extern PopupMenu* MenuAbierto;
        bool hay = false;
        if (MenuAbierto && MenuAbierto->abierto)
            for (size_t i = 0; i < MenuAbierto->items.size(); i++) if (MenuAbierto->items[i]->text == t) hay = true;
        printf("      [menuitem] '%s': %s\n", t.c_str(), hay ? "esta" : "NO esta");
        if (!hay) { err = "menuitem: el menu abierto no tiene '" + t + "'"; return false; }
        return true;
    }
    // 'propclic <fila en ingles>': un click de verdad en el valor de esa fila de Propiedades (una casilla, un boton)
    if (cmd == "propclic") {
        const std::string t = T(Resto(ss).c_str());
        int x = 0, y = 0;
        // si la fila esta pero fuera de la vista, se desplaza Propiedades con la rueda hasta que se vea (como la persona)
        Properties* P = (Properties*)HojaDeTipo(rootViewport, 3);
        for (int vuelta = 0; vuelta < 40; vuelta++) {
            PropertieBase* pb = NULL;
            if (FilaPropiedades(t, x, y, &pb)) break;
            if (!pb || !P) break;                              // no existe: no hay nada que desplazar
            P->event_mouse_wheel(y > P->y + P->height / 2 ? -1.0f : 1.0f, P->x + P->width / 2, P->y + P->height / 2);
            Cuadro();
        }
        if (!FilaPropiedades(t, x, y, NULL)) { err = "propclic: no se ve la fila '" + t + "' en Propiedades"; return false; }
        ClickEn(x, y, 1);
        return true;
    }
    // 'propboton <fila en ingles> | <texto>': lo que dice el boton de esa fila (el eje del Tornillo: 'Eje | Z')
    if (cmd == "propboton") {
        const std::string r = Resto(ss); const size_t bar = r.find('|');
        if (bar == std::string::npos) { err = "propboton: uso: propboton <fila> | <texto>"; return false; }
        std::string fila = r.substr(0, bar), txt = r.substr(bar + 1);
        while (!fila.empty() && fila[fila.size() - 1] == ' ') fila.erase(fila.size() - 1);
        while (!txt.empty() && txt[0] == ' ') txt.erase(0, 1);
        int x = 0, y = 0; PropertieBase* pb = NULL;
        FilaPropiedades(T(fila.c_str()), x, y, &pb);
        if (!pb || pb->GetType() != PropertyType::Button) { err = "propboton: no hay un boton '" + fila + "'"; return false; }
        const std::string hay = ((PropButton*)pb)->button->text;
        printf("      [propboton] %s = '%s' (esperaba '%s')\n", fila.c_str(), hay.c_str(), txt.c_str());
        if (hay != txt) { err = "propboton: '" + fila + "' dice '" + hay + "', se esperaba '" + txt + "'"; return false; }
        return true;
    }
    // 'modeje <x|y|z> <0|1>': el eje INTERNO (Y arriba) del modificador activo de la malla activa (Mirror)
    if (cmd == "modeje") {
        std::string e; int v = -1; ss >> e >> v;
        Mesh* m = (ObjActivo && ObjActivo->getType() == ObjectType::mesh) ? (Mesh*)ObjActivo : NULL;
        if (!m || m->modificadores.empty()) { err = "modeje: la malla activa no tiene modificadores"; return false; }
        int ia = m->modificadorActivo; if (ia < 0 || ia >= (int)m->modificadores.size()) ia = (int)m->modificadores.size() - 1;
        Modifier* mod = m->modificadores[ia];
        const bool hay = (e == "x") ? mod->ejeX : (e == "y") ? mod->ejeY : mod->ejeZ;
        printf("      [modeje] %s interno = %d (esperaba %d)\n", e.c_str(), hay ? 1 : 0, v);
        if ((hay ? 1 : 0) != v) { err = "modeje: el eje interno " + e + " no quedo como se esperaba"; return false; }
        return true;
    }
    // 'renombrarfila <objeto> <nombre nuevo>': como una persona: doble click en la fila del Outliner, escribe el
    // nombre (Shift en las mayusculas) y Enter
    if (cmd == "renombrarfila") {
        std::string nom, nuevo; ss >> nom >> nuevo;
        Outliner* o = (Outliner*)HojaDeTipo(rootViewport, 2);
        int x = 0, y = 0;
        if (!o || !FilaOutlinerDe(o, SceneCollection, nom, x, y)) { err = "renombrarfila: no se ve la fila '" + nom + "'"; return false; }
        ClickEn(x, y, 2);
        Escribir(nuevo);
        TeclaEv(SDLK_RETURN, true, 0); TeclaEv(SDLK_RETURN, false, 0);
        return true;
    }
    // 'vistalocal <objeto> <0|1>': ese objeto se ve (1) o no (0) en la vista local de la Vista 3D activa. Lo que
    // nace mientras dura (Separar con P) se tiene que ver: si no, desaparece y parece un error
    if (cmd == "vistalocal") {
        std::string nom; int v = -1; ss >> nom >> v;
        Viewport3D* vp = (Viewport3D*)HojaDeTipo(rootViewport, 1);
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
        if (!vp || !o) { err = "vistalocal: sin Vista 3D o sin objeto '" + nom + "'"; return false; }
        Cuadro();
        if (!vp->localViewActivo) { err = "vistalocal: la Vista 3D no esta en vista local"; return false; }
        const int hay = vp->localViewSet.count(o) ? 1 : 0;
        printf("      [vistalocal] '%s' %s en la vista local (esperaba %d)\n", nom.c_str(), hay ? "se ve" : "NO se ve", v);
        if (hay != v) { err = "vistalocal: '" + nom + "' no quedo como se esperaba en la vista local"; return false; }
        return true;
    }
    // 'elegirvertice abajo|arriba': en el Modo Edicion, elige SOLO el vertice mas bajo (o mas alto) de la malla, como
    // un click sobre el (el polo de abajo de una esfera: ahi va el cursor 3D para aplastarla desde el piso)
    if (cmd == "elegirvertice") {
        std::string q; ss >> q;
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "elegirvertice: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        if (!e || e->NumVerts() <= 0) { err = "elegirvertice: la malla no tiene vertices"; return false; }
        int mejor = 0;
        for (int k = 1; k < e->NumVerts(); k++) {
            const float y = e->pos[(size_t)k * 3 + 1], yb = e->pos[(size_t)mejor * 3 + 1];
            if (q == "arriba" ? y > yb : y < yb) mejor = k;
        }
        EditSelectMode = SelVertex;
        e->SeleccionarTodo(false);
        e->TogglearVert(mejor, true);
        g_redraw = true; Cuadro();
        return true;
    }
    // 'elegircerca <x> <y> <z>': en el Modo Edicion elige SOLO el elemento del modo actual (vertice, arista o cara) mas
    // cercano a ese punto de la malla (en local, como Propiedades: Z arriba). Es el click de la persona sobre "la cara de
    // adelante" o "la arista de afuera", sin depender de la camara
    if (cmd == "elegircerca") {
        float ux = 0, uy = 0, uz = 0; std::string sumar; ss >> ux >> uy >> uz >> sumar;
        const bool suma = (sumar == "+");        // '+': se suma a lo que ya estaba elegido (Shift + click)
        const float px = ux, py = uz, pz = uy;   // Z arriba -> interno (Y arriba)
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "elegircerca: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        if (!e) { err = "elegircerca: sin edit mesh"; return false; }
        int mejor = -1; float dMejor = 1e30f;
        if (EditSelectMode == SelFace) {
            for (int f = 0; f < e->NumFaces(); f++) {
                const float* c = &e->faceCenter[(size_t)f * 3];
                const float d = (c[0]-px)*(c[0]-px) + (c[1]-py)*(c[1]-py) + (c[2]-pz)*(c[2]-pz);
                if (d < dMejor) { dMejor = d; mejor = f; }
            }
            if (!suma) e->SeleccionarTodo(false); if (mejor >= 0) e->TogglearFace(mejor, !suma);
        } else if (EditSelectMode == SelEdge) {
            for (int a = 0; a < e->NumEdges(); a++) {
                const int k0 = e->lineIdx[(size_t)a * 2], k1 = e->lineIdx[(size_t)a * 2 + 1];
                const float* p0 = &e->pos[(size_t)k0 * 3]; const float* p1 = &e->pos[(size_t)k1 * 3];
                const float cx = (p0[0]+p1[0])*0.5f, cy = (p0[1]+p1[1])*0.5f, cz = (p0[2]+p1[2])*0.5f;
                const float d = (cx-px)*(cx-px) + (cy-py)*(cy-py) + (cz-pz)*(cz-pz);
                if (d < dMejor) { dMejor = d; mejor = a; }
            }
            if (!suma) e->SeleccionarTodo(false); if (mejor >= 0) e->TogglearEdge(mejor, !suma);
        } else {
            for (int k = 0; k < e->NumVerts(); k++) {
                const float* q = &e->pos[(size_t)k * 3];
                const float d = (q[0]-px)*(q[0]-px) + (q[1]-py)*(q[1]-py) + (q[2]-pz)*(q[2]-pz);
                if (d < dMejor) { dMejor = d; mejor = k; }
            }
            if (!suma) e->SeleccionarTodo(false); if (mejor >= 0) e->TogglearVert(mejor, !suma);
        }
        if (mejor < 0) { err = "elegircerca: no hay elementos"; return false; }
        printf("      [elegircerca] elemento %d a %.4f m\n", mejor, sqrtf(dMejor));
        g_redraw = true; Cuadro();
        return true;
    }
    // 'vinculado': agranda lo elegido a toda la pieza conectada (Seleccionar Vinculado, la L)
    if (cmd == "vinculado") {
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "vinculado: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        int k = -1;
        for (int i = 0; i < e->NumVerts() && k < 0; i++) if (i < (int)e->vertSel.size() && e->vertSel[i]) k = i;
        if (k < 0) for (int f = 0; f < e->NumFaces() && k < 0; f++) if (f < (int)e->faceSel.size() && e->faceSel[f] && !e->faces[f].empty()) k = e->faces[f][0];
        if (k < 0) for (int a = 0; a < e->NumEdges() && k < 0; a++) if (a < (int)e->edgeSel.size() && e->edgeSel[a]) k = e->lineIdx[(size_t)a * 2];
        if (k < 0) { err = "vinculado: no hay nada elegido"; return false; }
        e->SeleccionarLinked(k, EditSelectMode, false);
        g_redraw = true; Cuadro();
        return true;
    }
    // 'menuelegir <texto en ingles>': elige esa opcion del menu abierto, como Enter sobre ella (Shift+S y despues
    // "Cursor to Selected")
    if (cmd == "menuelegir") {
        const std::string t = T(Resto(ss).c_str());
        extern PopupMenu* MenuAbierto;
        PopupMenu* m = (MenuAbierto && MenuAbierto->abierto) ? MenuAbierto : NULL;
        int fila = -1;
        if (m) for (size_t i = 0; i < m->items.size(); i++) if (m->items[i]->text == t) fila = (int)i;
        if (fila < 0) { err = "menuelegir: el menu abierto no tiene '" + t + "'"; return false; }
        m->selectIndex = fila;
        const int id = m->Enter();
        if (id >= 0) m->Ejecutar(id);
        g_redraw = true; Cuadro();
        return true;
    }
    // 'vacioponer <forma> <tamanio> <rx> <ry> <rz>': prepara el dibujo del Vacio activo (para armar una escena, como
    // 'cursor3d' u 'origen'; lo que hace la persona se prueba con propclic + menuelegir)
    if (cmd == "vacioponer") {
        int fo = 2; float t = 1, rx = 0, ry = 0, rz = 0; ss >> fo >> t >> rx >> ry >> rz;
        if (!ObjActivo || ObjActivo->getType() != ObjectType::empty) { err = "vacioponer: el activo no es un Vacio"; return false; }
        Empty* e = (Empty*)ObjActivo;
        if (fo >= 0 && fo < VacioFormas) e->forma = fo;
        e->tamDibujo = t; e->rotDibujo = Vector3(rx, ry, rz);
        g_redraw = true; Cuadro();
        return true;
    }
    // 'vaciodibujo <forma> <tamanio> <rx> <ry> <rz>': el dibujo del Vacio activo (forma: 0 cruz, 1 cuadrado, 2 circulo,
    // 3 esfera). Lo que se elige en Propiedades y lo que vuelve al abrir el .w3d
    if (cmd == "vaciodibujo") {
        int fo = -1; float t = 0, rx = 0, ry = 0, rz = 0; ss >> fo >> t >> rx >> ry >> rz;
        if (!ObjActivo || ObjActivo->getType() != ObjectType::empty) { err = "vaciodibujo: el activo no es un Vacio"; return false; }
        const Empty* e = (const Empty*)ObjActivo;
        char b[200];
        snprintf(b, sizeof b, "vaciodibujo '%s': forma %d, tamanio %.3f, giro %.1f %.1f %.1f (esperaba %d, %.3f, %.1f %.1f %.1f)",
                 e->name.c_str(), e->forma, e->tamDibujo, e->rotDibujo.x, e->rotDibujo.y, e->rotDibujo.z, fo, t, rx, ry, rz);
        printf("      %s\n", b);
        if (e->forma != fo || fabsf(e->tamDibujo - t) > 1e-3f || fabsf(e->rotDibujo.x - rx) > 0.05f || fabsf(e->rotDibujo.y - ry) > 0.05f ||
            fabsf(e->rotDibujo.z - rz) > 0.05f) { err = b; return false; }
        return true;
    }
    // 'keyobj <frame> <x>': el objeto activo en X = x (interno) y un keyframe de todo en ese cuadro (la I)
    if (cmd == "keyobj") {
        int f = 1; float px = 0; ss >> f >> px;
        if (!ObjActivo) { err = "keyobj: sin objeto activo"; return false; }
        extern int CurrentFrame; extern void InsertarKeyframeObjeto(int);
        CurrentFrame = f; ObjActivo->pos.x = px; InsertarKeyframeObjeto(0);
        g_redraw = true; Cuadro();
        return true;
    }
    // 'curvapuntos <tipo> <n>': la forma de una curva (la MISMA cuenta del motor, KfEasing): n+1 puntos t -> fraccion
    // del camino. Para dibujarla al lado de la pelota que la usa
    if (cmd == "curvapuntos") {
        std::string q; int n = 32; ss >> q >> n;
        const int v = InterpDeNombre(q);
        if (v < 0 || n < 2) { err = "curvapuntos: uso: curvapuntos <tipo> <n>"; return false; }
        for (int i = 0; i <= n; i++) {
            const float t = (float)i / (float)n;
            const float y = (v == KfLinear) ? t : (v == KfBezier) ? t * t * (3.0f - 2.0f * t) : KfEasing(v, t);
            printf("      [curvapuntos] %.5f %.5f\n", t, y);
        }
        return true;
    }
    // 'keyponer <frame> <loc|esc> <x> <y> <z>': el objeto activo con esa posicion o esa escala (como Propiedades: Z
    // arriba) y un keyframe SOLO de eso en ese cuadro (I > Solo Localizacion / Solo Escala). Para armar una demo
    if (cmd == "keyponer") {
        int f = 1; std::string q; float ux = 0, uy = 0, uz = 0; ss >> f >> q >> ux >> uy >> uz;
        if (!ObjActivo) { err = "keyponer: sin objeto activo"; return false; }
        extern int CurrentFrame; extern void InsertarKeyframeObjeto(int);
        CurrentFrame = f;
        if (q == "loc") { ObjActivo->pos.x = ux; ObjActivo->pos.y = uz; ObjActivo->pos.z = uy; InsertarKeyframeObjeto(KfCanalLoc); }
        else if (q == "esc") { ObjActivo->scale.x = ux; ObjActivo->scale.y = uz; ObjActivo->scale.z = uy; InsertarKeyframeObjeto(KfCanalScl); }
        else { err = "keyponer: uso: keyponer <frame> loc|esc <x> <y> <z>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'keyinterp <lineal|bezier|in|out|inout> [frame] [loc|rot|esc]': la CURVA de los keyframes del objeto activo (Key >
    // Interpolacion / Easing): lineal (robotico), bezier, o cubica que acelera al arrancar (in), frena al llegar (out) o
    // las dos (inout). Con un cuadro, solo el tramo que SALE de ese cuadro; con loc/rot/esc, solo esas curvas
    // 'scriptval "<objeto>" <tarjeta> <prop> <valor>': lo MISMO que el panel de Propiedades al cambiar un valor de un
    // script (ScriptValGuardar): lo guarda en el objeto y, con el juego andando, se lo avisa a la sim (SimReresolver)
    // para que el script lo vea en ese momento, como el inspector de Unity
    if (cmd == "scriptval") {
        std::string resto; std::getline(ss, resto);
        size_t a = resto.find('"'), b2 = (a == std::string::npos) ? a : resto.find('"', a + 1);
        if (b2 == std::string::npos) { err = "scriptval: el objeto va entre comillas"; return false; }
        const std::string nom = resto.substr(a + 1, b2 - a - 1);
        std::istringstream r2(resto.substr(b2 + 1));
        int card = 0; std::string prop, valor; r2 >> card >> prop >> valor;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
        if (!o || !o->scriptDatos || card < 0 || card >= (int)o->scriptDatos->scripts.size()) {
            err = "scriptval: '" + nom + "' no tiene la tarjeta de script " + std::to_string(card); return false;
        }
        W3dScriptEntrada& e = o->scriptDatos->scripts[card];
        bool hay = false;
        for (size_t i = 0; i < e.refs.size(); i++) if (e.refs[i].first == prop) { e.refs[i].second = valor; hay = true; }
        if (!hay) e.refs.push_back(std::make_pair(prop, valor));
        extern bool SimActiva(); extern void SimReresolver(Object*);
        if (SimActiva()) SimReresolver(o);
        printf("      [scriptval] '%s' %s = %s%s\n", nom.c_str(), prop.c_str(), valor.c_str(), SimActiva() ? " (en vivo)" : "");
        g_redraw = true; Cuadro();
        return true;
    }
    // 'scriptref "<objeto>" <tarjeta> <prop> [esperado]': el valor guardado de una propiedad de ese script (lo del
    // inspector); '-' = no tiene valor propio (usa el de fabrica). Con 'esperado', falla si no es ese
    if (cmd == "scriptref") {
        std::string resto; std::getline(ss, resto);
        size_t a = resto.find('"'), b2 = (a == std::string::npos) ? a : resto.find('"', a + 1);
        if (b2 == std::string::npos) { err = "scriptref: el objeto va entre comillas"; return false; }
        const std::string nom = resto.substr(a + 1, b2 - a - 1);
        std::istringstream r2(resto.substr(b2 + 1));
        int card = 0; std::string prop, esperado; r2 >> card >> prop >> esperado;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nom) : NULL;
        if (!o || !o->scriptDatos || card < 0 || card >= (int)o->scriptDatos->scripts.size()) {
            err = "scriptref: '" + nom + "' no tiene la tarjeta de script " + std::to_string(card); return false;
        }
        std::string hay = "-";
        const W3dScriptEntrada& e = o->scriptDatos->scripts[card];
        for (size_t i = 0; i < e.refs.size(); i++) if (e.refs[i].first == prop) hay = e.refs[i].second;
        printf("      [scriptref] '%s' [%d] %s = %s\n", nom.c_str(), card, prop.c_str(), hay.c_str());
        if (!esperado.empty() && hay != esperado) { err = "scriptref: se esperaba " + esperado; return false; }
        return true;
    }
    if (cmd == "keyinterp") {
        std::string q, qp; int fsolo = -1; ss >> q; if (!(ss >> fsolo)) fsolo = -1; ss >> qp;
        const int psolo = (qp == "loc") ? AnimPosition : (qp == "rot") ? AnimRotation : (qp == "esc") ? AnimScale : -1;
        const int v = InterpDeNombre(q);
        if (v < 0) { err = "keyinterp: lineal|bezier|in|out|inout|cuadratica|rebote|elastica [frame] [loc|rot|esc]"; return false; }
        int n = 0;
        for (size_t a = 0; a < AnimationObjects.size(); a++) {
            if (AnimationObjects[a].obj != ObjActivo) continue;
            for (size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
                if (psolo >= 0 && AnimationObjects[a].Propertys[p].Property != psolo) continue;
                for (size_t k = 0; k < AnimationObjects[a].Propertys[p].keyframes.size(); k++) {
                    if (fsolo >= 0 && AnimationObjects[a].Propertys[p].keyframes[k].frame != fsolo) continue;
                    AnimationObjects[a].Propertys[p].keyframes[k].Interpolation = v; n++;
                }
            }
        }
        if (!n) { err = "keyinterp: el objeto activo no tiene keyframes"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'tl selframe <f> | duplicar | copiar | pegar | valor <texto> | aceptar | cancelar | frame <f>': la LINEA DE TIEMPO
    // por sus funciones de siempre (las de Shift+D, Ctrl+C, Ctrl+V, el numero tipeado mientras se mueve, Enter, Esc)
    if (cmd == "tl") {
        std::string q; ss >> q;
        Timeline* tl = (Timeline*)HojaDeTipo(rootViewport, 5);
        if (!tl) { err = "tl: no hay una Linea de tiempo"; return false; }
        extern int CurrentFrame;
        tl->ConstruirDopeRows();
        if (q == "selframe") { int f = 0; ss >> f; tl->DopeSelectNone(); tl->DopeSelFrame(f, true); }
        else if (q == "duplicar") tl->DopeDuplicarSeleccion();
        else if (q == "copiar") tl->DopeCopiarSeleccion();
        else if (q == "pegar") tl->DopePegar();
        else if (q == "valor") { std::string t; ss >> t; for (size_t i = 0; i < t.size(); i++) tl->DopeNumChar((unsigned char)t[i]); tl->DopeNumAplicar(); }
        else if (q == "aceptar") tl->DopeMoveConfirm();
        else if (q == "cancelar") tl->DopeMoveCancel();
        else if (q == "frame") { int f = 0; ss >> f; CurrentFrame = f; }
        else if (q == "curvas") tl->CambiarModo(true);    // el EDITOR DE CURVAS (View > Curvas)
        else if (q == "dope") tl->CambiarModo(false);
        else if (q == "fin") { int f = 0; ss >> f; tl->fEnd = (float)f; tl->ApplyEnd(); }   // el Fin del reloj
        else if (q == "encuadrar") {   // el editor de curvas encuadra TODAS sus curvas (tiempo y valor), como View
            float f0 = 0, f1 = 0, v0 = 0, v1 = 0;
            if (tl->CurvaRangoVista(false, f0, f1, v0, v1)) tl->CurvaEncuadrar(f0, f1, v0, v1);
        }
        else if (q == "vista") {   // 'tl vista <frame izq> <frame der> [valor abajo] [valor arriba]': el encuadre
            float f0 = 0, f1 = 30, v0 = 0, v1 = 0; ss >> f0 >> f1; const bool conV = (bool)(ss >> v0 >> v1);
            const float anchoStrip = (float)(tl->width - tl->panelW);
            const float zf = anchoStrip / ((f1 - f0) > 0.5f ? (f1 - f0) : 0.5f);
            float zv = tl->pxPerUnit, cv = tl->viewCenterV;
            if (conV && v1 > v0) { zv = (float)(tl->height - tl->stripY) / (v1 - v0); cv = (v0 + v1) * 0.5f; }
            tl->VistaAplicar(zf, f0, zv, cv);
        }
        else if (q == "borrarcurva") {   // 'tl borrarcurva <loc|rot|esc> <x|y|z>' (eje como Propiedades: Z arriba)
            std::string qp, qe; ss >> qp >> qe;
            const int p = (qp == "loc") ? AnimPosition : (qp == "rot") ? AnimRotation : (qp == "esc") ? AnimScale : -1;
            const int c = (qe == "x") ? AnimX : (qe == "y") ? AnimZ : (qe == "z") ? AnimY : -1;   // Z arriba -> Y interno
            int n = 0;
            for (size_t a = 0; a < AnimationObjects.size(); a++) {
                if (AnimationObjects[a].obj != ObjActivo) continue;
                for (size_t k = 0; k < AnimationObjects[a].Propertys.size(); k++)
                    if (AnimationObjects[a].Propertys[k].Property == p && AnimationObjects[a].Propertys[k].component == c) {
                        AnimationObjects[a].Propertys[k].keyframes.clear(); n++;
                    }
            }
            if (!n) { err = "tl borrarcurva: el objeto activo no tiene esa curva"; return false; }
        }
        else { err = "tl: uso: tl selframe <f>|duplicar|copiar|pegar|valor <texto>|aceptar|cancelar|frame <f>|fin <f>|borrarcurva <p> <eje>"; return false; }
        g_redraw = true; Cuadro();
        return true;
    }
    // 'keyvalor <loc|rot|esc> <x|y|z> <cuadro> <valor> [interp]': el objeto activo tiene un keyframe en ese cuadro de
    // esa curva, con ese valor (+-0.001) y, si se pasa, esa interpolacion (el int tal cual: 2 bezier, 261 cubica al
    // arrancar, 517 cubica al llegar). El eje es el de la Vista 3D (Z arriba: S Z escala lo vertical). Lo que deja un
    // tutorial de animacion
    if (cmd == "keyvalor") {
        std::string qp, qe; int f = 0; float val = 0; int interp = -1; ss >> qp >> qe >> f >> val; if (!(ss >> interp)) interp = -1;
        if (!ObjActivo) { err = "keyvalor: sin objeto activo"; return false; }
        const int p = (qp == "loc") ? AnimPosition : (qp == "rot") ? AnimRotation : (qp == "esc") ? AnimScale : -1;
        const int c = (qe == "x") ? AnimX : (qe == "y") ? AnimZ : (qe == "z") ? AnimY : -1;   // Z arriba -> Y interno
        const keyFrame* kf = NULL;
        for (size_t a = 0; a < AnimationObjects.size() && !kf; a++) {
            if (AnimationObjects[a].obj != ObjActivo) continue;
            for (size_t k = 0; k < AnimationObjects[a].Propertys.size() && !kf; k++) {
                const AnimProperty& ap = AnimationObjects[a].Propertys[k];
                if (ap.Property != p || ap.component != c) continue;
                for (size_t i = 0; i < ap.keyframes.size(); i++) if (ap.keyframes[i].frame == f) { kf = &ap.keyframes[i]; break; }
            }
        }
        char b[200];
        if (!kf) { snprintf(b, sizeof b, "keyvalor: no hay keyframe de %s %s en el cuadro %d", qp.c_str(), qe.c_str(), f); err = b; return false; }
        snprintf(b, sizeof b, "keyvalor %s %s @%d = %.4f/%d (esperaba %.4f/%d)", qp.c_str(), qe.c_str(), f, kf->value, kf->Interpolation, val, interp);
        printf("      %s\n", b);
        if (fabsf(kf->value - val) > 0.001f || (interp >= 0 && kf->Interpolation != interp)) { err = b; return false; }
        return true;
    }
    // 'keyver': imprime las curvas del objeto activo (propiedad, eje interno, y cada keyframe: cuadro=valor/interp).
    // Para comparar lo que deja el tutorial con el .w3d de la persona
    if (cmd == "keyver") {
        if (!ObjActivo) { err = "keyver: sin objeto activo"; return false; }
        for (size_t a = 0; a < AnimationObjects.size(); a++) {
            if (AnimationObjects[a].obj != ObjActivo) continue;
            for (size_t k = 0; k < AnimationObjects[a].Propertys.size(); k++) {
                const AnimProperty& ap = AnimationObjects[a].Propertys[k];
                if (ap.keyframes.empty()) continue;
                printf("      [keyver] p=%d c=%d:", ap.Property, ap.component);
                for (size_t i = 0; i < ap.keyframes.size(); i++)
                    printf(" %d=%.3f/%d", ap.keyframes[i].frame, ap.keyframes[i].value, ap.keyframes[i].Interpolation);
                printf("\n");
            }
        }
        extern int StartFrame, EndFrame;
        printf("      [keyver] rango %d a %d\n", StartFrame, EndFrame);
        return true;
    }
    // 'objkeys <objeto> [n]': cuantos keyframes de ESCENA tiene el objeto (de la raiz activa) sumando TODAS sus
    // animaciones de escena (la activa vive en AnimationObjects; las otras, en su lista). Con n, falla si no son esos.
    // ("Convert to Game": la copia del juego no tiene ninguno y la escena conserva los suyos)
    if (cmd == "objkeys") {
        std::string nm; int esperado = -1; ss >> nm >> esperado;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o) { err = "objkeys: no hay un objeto '" + nm + "'"; return false; }
        struct K { static int De(const std::vector<AnimationObject>& v, Object* o) {
            int n = 0;
            for (size_t a = 0; a < v.size(); a++)
                if (v[a].obj == o)
                    for (size_t k = 0; k < v[a].Propertys.size(); k++) n += (int)v[a].Propertys[k].keyframes.size();
            return n;
        } };
        int n = K::De(AnimationObjects, o);
        for (size_t e = 0; e < SceneAnimations.size(); e++)
            if ((int)e != SceneAnimActiva && SceneAnimations[e]) n += K::De(SceneAnimations[e]->objetos, o);
        printf("      [objkeys] %s: %d keyframe(s) de escena (%d animacion(es) de escena)\n", nm.c_str(), n,
               (int)SceneAnimations.size());
        if (esperado >= 0 && n != esperado) {
            err = "objkeys: " + nm + " tiene " + std::to_string(n) + " keyframe(s), se esperaban " + std::to_string(esperado);
            return false;
        }
        return true;
    }
    // 'objscript <objeto> <fila> [ruta R] [apagado 0|1] [ref P V]...': el script de esa fila de un objeto de la raiz
    // activa: su .lua, su ojo y sus valores (los asserta)
    if (cmd == "objscript") {
        std::string nm; int fila = -1; ss >> nm >> fila;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o || !o->scriptDatos || fila < 0 || fila >= (int)o->scriptDatos->scripts.size()) {
            err = "objscript: '" + nm + "' no tiene el script " + std::to_string(fila); return false;
        }
        const W3dScriptEntrada& e = o->scriptDatos->scripts[(size_t)fila];
        printf("      [objscript] %s #%d: %s%s, %d valor(es)\n", nm.c_str(), fila, e.ruta.c_str(), e.apagado ? " (APAGADO)" : "",
               (int)e.refs.size());
        std::string k;
        while (ss >> k) {
            if (k == "ruta") {
                std::string r; ss >> r;
                if (e.ruta != r) { err = "objscript: el script es '" + e.ruta + "', se esperaba '" + r + "'"; return false; }
            } else if (k == "apagado") {
                int a = -1; ss >> a;
                if ((e.apagado ? 1 : 0) != a) { err = std::string("objscript: el script esta ") + (e.apagado ? "apagado" : "prendido"); return false; }
            } else if (k == "ref") {
                std::string p, v; ss >> p >> v;
                bool hay = false;
                for (size_t r = 0; r < e.refs.size(); r++)
                    if (e.refs[r].first == p) {
                        hay = true;
                        if (e.refs[r].second != v) { err = "objscript: " + p + " = '" + e.refs[r].second + "', se esperaba '" + v + "'"; return false; }
                    }
                if (!hay) { err = "objscript: el script no tiene el valor '" + p + "'"; return false; }
            } else { err = "objscript: no entiendo '" + k + "'"; return false; }
        }
        return true;
    }
    // 'objfisica <objeto> poner <estatico|dinamico|personaje|malla> <masa>': le pone un cuerpo rigido (lo de la tarjeta
    // Fisica); 'objfisica <objeto> <tipo|-> [masa M]': asserta el que tiene ('-' = ninguno)
    if (cmd == "objfisica") {
        static const char* kTipos[4] = { "estatico", "dinamico", "personaje", "malla" };
        std::string nm, t; ss >> nm >> t;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o) { err = "objfisica: no hay un objeto '" + nm + "'"; return false; }
        if (t == "poner") {
            std::string q; float masa = 1.0f; ss >> q >> masa;
            int tipo = -1;
            for (int i = 0; i < 4; i++) if (q == kTipos[i]) tipo = i;
            if (tipo < 0) { err = "objfisica: uso: objfisica <objeto> poner estatico|dinamico|personaje|malla <masa>"; return false; }
            if (!o->fisica) o->fisica = new W3dRigidoDef();
            o->fisica->tipo = tipo; o->fisica->masa = masa;
            return true;
        }
        const std::string hay = o->fisica ? std::string(kTipos[o->fisica->tipo & 3]) : std::string("-");
        printf("      [objfisica] %s: %s", nm.c_str(), hay.c_str());
        if (o->fisica) printf(" masa %.3f", o->fisica->masa);
        printf("\n");
        if (hay != t) { err = "objfisica: " + nm + " tiene '" + hay + "', se esperaba '" + t + "'"; return false; }
        std::string k;
        while (ss >> k) {
            float m = 0.0f; ss >> m;
            if (k == "masa" && o->fisica && fabsf(o->fisica->masa - m) > 1e-4f) { err = "objfisica: otra masa"; return false; }
        }
        return true;
    }
    // 'tiraesqueleto <armature> <clip> <frame>': una TIRA de esqueleto en la animacion de escena activa (el armature toca
    // ese clip desde ese frame de la escena: una cinematica)
    // 'esqueletoclip <armature> <clip|-> [cabezal F]': el clip ACTIVO del armature (el que reproduce en "Juego") y su
    // cabezal de juego (juegoFrame: frames desde el inicio del clip)
    if (cmd == "tiraesqueleto" || cmd == "esqueletoclip") {
        std::string nm, clip; ss >> nm >> clip;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o || o->getType() != ObjectType::armature) { err = cmd + ": no hay un armature '" + nm + "'"; return false; }
        Armature* a = (Armature*)o;
        if (cmd == "tiraesqueleto") {
            int f = 1; ss >> f;
            InitSceneAnimations();
            if (SceneAnimActiva < 0 || SceneAnimActiva >= (int)SceneAnimations.size() || W3dArmatureClipPorNombre(a, clip) < 0) {
                err = "tiraesqueleto: '" + nm + "' no tiene el clip '" + clip + "'"; return false;
            }
            SceneAnimation::PistaEsqueleto p; p.armature = a->name; p.clip = clip; p.frame = f;
            SceneAnimations[(size_t)SceneAnimActiva]->esqueletos.push_back(p);
            return true;
        }
        const std::string hay = (a->animActiva >= 0 && a->animActiva < (int)a->animations.size() && a->animations[(size_t)a->animActiva])
                                    ? a->animations[(size_t)a->animActiva]->name : std::string("-");
        printf("      [esqueletoclip] %s: clip %s cabezal %.3f\n", nm.c_str(), hay.c_str(), a->juegoFrame);
        if (hay != clip) { err = "esqueletoclip: el clip activo de " + nm + " es '" + hay + "', se esperaba '" + clip + "'"; return false; }
        std::string k; float v = 0.0f;
        if (ss >> k >> v && k == "cabezal" && fabsf(a->juegoFrame - v) > 1e-3f) {
            err = "esqueletoclip: el cabezal de " + nm + " es " + std::to_string(a->juegoFrame) + ", se esperaba " + std::to_string(v);
            return false;
        }
        return true;
    }
    // 'tlframes <f1> <f2> ...': la Linea de tiempo tiene keyframes EXACTAMENTE en esos cuadros
    if (cmd == "tlframes") {
        Timeline* tl = (Timeline*)HojaDeTipo(rootViewport, 5);
        if (!tl) { err = "tlframes: no hay una Linea de tiempo"; return false; }
        tl->ConstruirDopeRows();
        std::vector<int> hay; tl->DopeFramesNav(hay);
        std::vector<int> esp; int f; while (ss >> f) esp.push_back(f);
        std::string sh, se; char b[16];
        for (size_t i = 0; i < hay.size(); i++) { snprintf(b, sizeof b, "%d ", hay[i]); sh += b; }
        for (size_t i = 0; i < esp.size(); i++) { snprintf(b, sizeof b, "%d ", esp[i]); se += b; }
        printf("      [tlframes] hay: %s(esperaba: %s)\n", sh.c_str(), se.c_str());
        if (hay != esp) { err = "tlframes: hay keyframes en " + sh + "y se esperaba " + se; return false; }
        return true;
    }
    // 'gizmomodo <0|1|2>': el gizmo en mover / rotar / escalar
    if (cmd == "gizmomodo") { int mo = 0; ss >> mo; GizmoSetModo(mo); g_redraw = true; Cuadro(); return true; }
    // 'dedogizmo <manija> <dx> <dy> <pasos>': el DEDO agarra esa manija del gizmo y la arrastra despacio, con la espera
    // de un toque de verdad. La camara NO se tiene que mover en ningun momento (antes orbitaba hasta pasar el umbral y
    // recien ahi giraba el objeto) y el objeto si tiene que cambiar
    if (cmd == "dedogizmo") {
        int man = -1, dx = 0, dy = 0, pasos = 10; ss >> man >> dx >> dy >> pasos;
        Viewport3D* vp = (Viewport3D*)HojaDeTipo(rootViewport, 1);
        if (!vp || !ObjActivo) { err = "dedogizmo: sin Vista 3D o sin objeto activo"; return false; }
        Cuadro();
        float sx = 0, sy = 0;
        if (!GizmoManijaEnPantalla(vp, man, sx, sy)) { err = "dedogizmo: la manija no se ve"; return false; }
        const int x0 = vp->x + (int)(sx + 0.5f), y0 = vp->y + (int)(sy + 0.5f);
        const Vector3 cam0 = vp->viewPos;
        const Quaternion r0 = ObjActivo->Rot(); const Vector3 p0 = ObjActivo->pos, e0 = ObjActivo->scale;
        Movio(SDL_TOUCH_MOUSEID, 0, x0, y0, x0, y0); Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x0, y0); Dedo(SDL_FINGERDOWN, 1, x0, y0, x0, y0);
        SDL_Delay(250);   // (pasada la espera anti-gesto: un dedo que se queda un momento antes de arrastrar)
        int px = x0, py = y0; bool camaraQuieta = true;
        if (pasos < 1) pasos = 1;
        for (int k = 1; k <= pasos; k++) {
            const int x = x0 + dx * k / pasos, y = y0 + dy * k / pasos;
            Movio(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, x, y, px, py); Dedo(SDL_FINGERMOTION, 1, x, y, px, py);
            px = x; py = y;
            const Vector3 d = vp->viewPos - cam0;
            if (fabsf(d.x) + fabsf(d.y) + fabsf(d.z) > 1e-4f) camaraQuieta = false;
            SDL_Delay(15);
        }
        Boton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, px, py); Dedo(SDL_FINGERUP, 1, px, py, px, py);
        const Quaternion r1 = ObjActivo->Rot();
        const bool cambio = fabsf(r1.w - r0.w) + fabsf(r1.x - r0.x) + fabsf(r1.y - r0.y) + fabsf(r1.z - r0.z) > 1e-4f ||
                            (ObjActivo->pos - p0).Length() > 1e-4f || (ObjActivo->scale - e0).Length() > 1e-4f;
        printf("      [dedogizmo] la camara %s, el objeto %s\n", camaraQuieta ? "quieta" : "SE MOVIO", cambio ? "cambio" : "NO cambio");
        if (!camaraQuieta) { err = "dedogizmo: la camara orbito mientras el dedo arrastraba el gizmo"; return false; }
        if (!cambio) { err = "dedogizmo: el objeto no cambio"; return false; }
        return true;
    }
    manejado = false;
    return false;
}
