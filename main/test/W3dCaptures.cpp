// Comandos de harness para las capturas de los tutoriales (ver W3dCaptures.h y tools/capturas/README.md).
#include "test/W3dCaptures.h"
#include "test/W3dHistory.h"          // W3dHistoryFrame (la grabadora de pasos, tambien en el harness)
#include "app/controls.h"             // SDL_Event + InputUserSDL3: la puerta real de la entrada
#include "app/variables.h"            // window
#include "ViewPorts/ViewPorts.h"      // ViewportBase / ViewportColumn (el arbol del layout)
#include "w3dTexture.h"                // SavePNG (buffershot)
#include "ViewPorts/ViewPort3D.h"     // Viewport3D (layoutpordefecto)
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/core/UI.h"         // RowHeightGS, gapGS, borderGS (las filas del menu)
#include "render/RenderOptions.h"    // g_redraw
#include "render/RayTracer.h"        // g_rt / RTInvalidate (vista3d trazado)
#include "objects/Objects.h"         // g_editMesh, EditSelectMode (caras)
#include "objects/Mesh.h"            // Mesh (caras)
#include "edit/Modifier.h"           // Modifier (modeje)
#include "objects/Empty.h"           // el dibujo del Vacio (vaciodibujo)
#include "animation/Animation.h"     // las curvas de los keyframes (keyinterp)
#include "script/W3dScript.h"        // W3dScriptData / W3dScriptEntry (scriptval: los valores de un script)
#include "physics/W3dRigid.h"        // W3dRigidDef (objfisica)
#include "objects/Armature.h"        // tiraesqueleto / esqueletoclip
#include "animation/SkeletalAnimation.h"   // W3dArmatureClipByName (tiraesqueleto)
// el centro del ojo de un script de la lista (Properties.cpp): capinfo y ojoscript
class Properties;
bool PropsEyeScriptCenter(Properties* p, int row, int* cx, int* cy);
class Button;
Button* PropsHelpIn(Properties* p, int mx, int my);   // el cartelito de ayuda de Propiedades (Properties.cpp)
#include "WhiskUI/text/font.h"         // WhiskFont->Supports (cobertura)
#include "config/W3dLang.h"           // los idiomas (cobertura)
#include "w3dFilesystem.h"            // leer res/languages (cobertura)
#include <set>
#include "objects/EditMesh.h"         // EditMesh (caras)
#include <stdio.h>
#include <string.h>
#include <map>
#include <math.h>
#include "importers/stl.h"         // ExportSTL (stl)
#include "objects/ObjectMode.h"     // BeginTranslate / BeginRotate / SetScale / CycleAxisTransform (n95 transformar)
#include "ViewPorts/Gizmo.h"         // GizmoHandleInScreen (capinfo: las manijas)
#include "ViewPorts/Properties.h"    // PropsActive y los tildes de los pases (renderpases / propsolvidar)
#include "stb/stb_image.h"            // leer un PNG del render (pngpx; la implementacion esta en constructor.cpp)
#include "importers/import_obj.h"   // LoadAllPendingTextures (texturas)
#include "undo/Undo.h"               // UndoConstraintAdd / UndoModAdd (agregar)
#include "ViewPorts/Outliner.h"      // las filas (capinfo)
#include "ViewPorts/PopUp/FileBrowser.h"   // el explorador de archivos (capinfo: sus archivos y botones)
#include "ViewPorts/PopUp/SaveAsPopup.h"   // guardarcomofijar: llenar el "Guardar como..." sin guardar
#include "ViewPorts/Timeline.h"      // la linea de tiempo (tl, tlframes)
#include "ViewPorts/Properties.h"    // las tarjetas y filas (capinfo)
#include "ViewPorts/PopUp/NumPad.h"  // NumPadKeyCenter (capinfo)
#include "ViewPorts/LayoutInput.h"   // LayoutKey (tecladotocar: el OK del QWERTY)
#include "WhiskUI/draw/icons.h"     // IconIndex (dedotoque boton <icono>)
#include "WhiskUI/widgets/Tab.h"
#include "WhiskUI/Propieties/GroupPropertie.h"
#ifdef _MSC_VER   // (ffprobe de mediapistas / videocuadros: en Visual C++ popen se llama _popen)
#define popen  _popen
#define pclose _pclose
#endif
extern int OutlinerObjectRow(Object* o);   // Outliner.cpp: la fila visible de un objeto en la escena (-1)
extern int OutlinerYOfRow(int row);        // Outliner.cpp: su Y desde arriba del arbol (con los cortes del constructor)

extern ViewportBase* rootViewport;
extern int MenuScreenW, MenuScreenH;
extern int W3dScreenHeight;
// la tecla VERDE del N95 (platform/symbian/src/Whisk3DContainer.cpp): las MISMAS funciones que llama el telefono
void LayoutCycleActiveViewport(int dir);                            // verde sola (al soltarla)
void LayoutResizeActiveViewport(int dx, int dy, float step);        // verde + flecha, una vez por cuadro
ViewportBase* LayoutDefault(int w, int h, Viewport3D** vp3dOut);    // LayoutInput.cpp: el del arranque
extern Viewport3D* Viewport3DActive;
extern bool LShiftPressed, LAltPressed, LCtrlPressed;   // controls.cpp (los modificadores que mira el viewport 3D)
void LayoutRenderMenu(int w, int h);
void SetGlobalScale(int scale);        // ViewPorts.cpp: re-deriva las metricas *GS   // LayoutInput.cpp: los desplegables y popups (van encima de todo)

namespace {
    // la ultima posicion de cada puntero: SDL manda el desplazamiento (xrel/yrel) en cada movimiento, y el
    // arrastre del boton de la esquina redimensiona con ESO, no con la posicion
    int gMouseX = 0, gMouseY = 0;
    Uint32 gMouseButtons = 0;
    std::map<int, std::pair<int, int> > gFingers;   // la ultima posicion de cada dedo apoyado (por id)
    int gModeN95 = 0;   // el transform del keypad en curso (n95 transformar): 0 nada, 1 mover, 2 rotar, 3 escalar

    // UN CUADRO, como el loop del editor entre evento y evento: el hit-test de los botones usa la posicion
    // que les dejo el ultimo dibujado (sin esto, el primer click no le acertaba a la esquina: sx/sy en 0)
    void Frame() {
        if (!rootViewport) return;
        // como el loop del editor (main.cpp): los keyframes se aplican al cuadro actual. Sin esto, un click en los
        // numeros de la Linea de tiempo cambiaba el cuadro pero el objeto seguia con la pose del cuadro anterior (y una
        // S despues escalaba esa pose). Solo trabaja al cambiar de cuadro
        ::ApplyAnimationObjects();   // (ObjectMode.h)
        // (como el loop: navegando una vista 3D, los paneles de la imagen guardada y solo las vistas 3D)
        const bool only3D = W3dViewCacheBegin(rootViewport, MenuScreenW, W3dScreenHeight);
        if (only3D) W3dViewCacheRender3D(rootViewport);
        else rootViewport->Render();
        W3dViewCacheEnd(MenuScreenW, W3dScreenHeight, only3D);
        LayoutRenderMenu(MenuScreenW, MenuScreenH);
#ifdef W3D_MCP
        W3dHistoryFrame();   // la grabadora de pasos (W3D_HISTORY): como en el loop del editor
#endif
    }

    bool gFirst = true;
    void Dispatch(SDL_Event& e) {
        if (gFirst) { gFirst = false; Frame(); }
        g_redraw = true;
        InputUserSDL3(e);
        Frame();
    }

    int ButtonOf(const std::string& s) {
        if (s == "right") return SDL_BUTTON_RIGHT;
        if (s == "middle") return SDL_BUTTON_MIDDLE;
        return SDL_BUTTON_LEFT;
    }

    // el editor lee la posicion del mouse de SDL (SDL_GetMouseState) al apretar un boton (SaveMousePos): el delta
    // del arrastre se mide desde ahi. Los eventos sintetizados no la tocan: se lleva el cursor de SDL a mano (en el
    // driver 'offscreen' solo cambia ese estado). Sin esto el primer movimiento de un arrastre venia desde (0, 0)
    // y la vista pegaba un salto enorme.
    void TakeCursor(int x, int y) { if (window) SDL_WarpMouseInWindow(window, (float)x, (float)y); }

    void Moved(Uint32 which, Uint32 state, int x, int y, int px, int py) {
        TakeCursor(x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEMOTION;
        e.motion.timestamp = SDL_GetTicks();
        e.motion.which = which;
        e.motion.state = state;
        e.motion.x = x; e.motion.y = y; e.motion.xrel = x - px; e.motion.yrel = y - py;
        Dispatch(e);
    }

    int gClicks = 1;   // 'mouse doble': el 2do click llega con clicks = 2 (SDL), que es lo que controls.cpp mira
    void SendButton(Uint32 which, int b, bool down, int x, int y) {
        TakeCursor(x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.timestamp = SDL_GetTicks();
        e.button.which = which;
        e.button.button = (Uint8)b;
        e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = (Uint8)gClicks;
        e.button.x = x; e.button.y = y;
        Dispatch(e);
    }

    void SendFinger(Uint32 type, int id, int x, int y, int px, int py) {
        int ww = 0, hh = 0; if (window) SDL_GetWindowSize(window, &ww, &hh);
        if (ww <= 0 || hh <= 0) { ww = MenuScreenW; hh = MenuScreenH; }
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = type;
        e.tfinger.timestamp = SDL_GetTicks();
        e.tfinger.fingerId = (SDL_FingerID)id;
        // controls.cpp lo pasa a pixeles con el tamanio de la VENTANA: se normaliza con el mismo
        // (+0.5: que el (int) de vuelta caiga en el mismo pixel)
        e.tfinger.x = ((float)x + 0.5f) / (float)ww;  e.tfinger.y = ((float)y + 0.5f) / (float)hh;
        e.tfinger.dx = (float)(x - px) / (float)ww;   e.tfinger.dy = (float)(y - py) / (float)hh;
        e.tfinger.pressure = (type == SDL_FINGERUP) ? 0.0f : 1.0f;
        Dispatch(e);
    }

    // la rueda va al viewport bajo el cursor, y controls.cpp lo lee de SDL_GetMouseState: se lleva el
    // cursor ahi primero (en el driver 'offscreen' no hay cursor de verdad: solo cambia el estado de SDL)
    void Wheel(int steps, int x, int y) {
        if (window) SDL_WarpMouseInWindow(window, x, y);
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEWHEEL;
        e.wheel.timestamp = SDL_GetTicks();
        e.wheel.which = 0;
        e.wheel.y = steps;
        Dispatch(e);
    }

    // la misma cuenta que el harness del Outliner (W3dTestsOutliner.cpp, PointByName): el centro del NOMBRE de la fila
    void InfoRows(Outliner* o, Object* root) {
        if (!root) return;
        for (size_t i = 0; i < root->Childrens.size(); i++) {
            Object* ob = root->Childrens[i];
            const int row = OutlinerObjectRow(ob);
            if (row >= 0) {
                int prof = 0;
                for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
                const int x = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
                const int y = o->y + borderGS + o->PosY + o->BarTopOffset() + OutlinerYOfRow(row) + (int)RowHeightGS / 2;
                const int reserve = o->scrollY ? (GlobalScale * 9 + gapGS) : 0;   // (Outliner.cpp: la barra de desplazamiento)
                const int eye = o->x + o->width - 2 * IconSizeGS - gapGS - marginGS - borderGS - reserve + IconSizeGS / 2;
                const int cam = o->x + o->width - IconSizeGS - marginGS - borderGS - reserve + IconSizeGS / 2;
                const int arrow = o->x + marginGS + o->PosX + prof * (IconSizeGS + gapGS) + IconSizeGS / 2;
                if (y >= o->y && y < o->y + o->height)
                    printf("@cap fila x=%d y=%d prof=%d ojo=%d camara=%d flecha=%d elegido=%d name=%s\n", x, y, prof, eye, cam, arrow,
                           ob->select ? 1 : 0, ob->name.c_str());
            }
            InfoRows(o, ob);
        }
    }

    void InfoObjects(Viewport3D* vp, Object* root) {
        if (!root) return;
        for (size_t i = 0; i < root->Childrens.size(); i++) {
            Object* ob = root->Childrens[i];
            float sx = 0, sy = 0;
            if (ob->getType() != ObjectType::collection && vp->ProjectPoint(ob->GetGlobalPositionBase(), sx, sy))
                printf("@cap objeto x=%d y=%d elegido=%d name=%s\n", vp->x + (int)(sx + 0.5f), vp->y + (int)(sy + 0.5f),
                       ob->select ? 1 : 0, ob->name.c_str());
            InfoObjects(vp, ob);
        }
    }

    // el nombre de una curva en las recetas -> la Interpolation del keyframe (-1 = no existe)
    int InterpByName(const std::string& q) {
        if (q == "linear") return KfLinear;
        if (q == "bezier") return KfBezier;
        if (q == "in") return KfInterpWith(KfCubic, KfEaseIn);
        if (q == "out") return KfInterpWith(KfCubic, KfEaseOut);
        if (q == "inout") return KfInterpWith(KfCubic, KfEaseInOut);
        if (q == "cuadratica") return KfQuad;                 // (Automatica: Al arrancar)
        if (q == "bounce") return KfBounce;                   // (Automatica: Al llegar)
        if (q == "elastica") return KfElastic;                // (Automatica: Al llegar)
        return -1;
    }
    void InfoLeaves(ViewportBase* n) {
        if (!n) return;
        if (!n->isLeaf()) {
            InfoLeaves(((ViewportColumn*)n)->childA);
            InfoLeaves(((ViewportColumn*)n)->childB);
            return;
        }
        n->UpdateBar();
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
            Button* bv = BarRoleBtn(n->BarButtons, BR_View);
            if (bv && bv->visible) printf("@cap botonvista x=%d y=%d w=%d h=%d\n", bv->sx, bv->sy, bv->width, bv->height);
        }
        // el candado de la vista de camara (si se ve): para apuntarle con el mouse o el dedo
        if (n->ViewportKind() == 1 && ((Viewport3D*)n)->LockCameraVisible()) {
            int rx = 0, ry = 0, side = 0; ((Viewport3D*)n)->LockCameraRect(rx, ry, side);
            printf("@cap candado x=%d y=%d lado=%d\n", n->x + rx, n->y + ry, side);
        }
        for (size_t i = 0; i < n->BarButtons.size(); i++) {
            Button* b = n->BarButtons[i];
            printf("@cap boton kind=%d i=%d rol=%d visible=%d x=%d y=%d w=%d h=%d texto=%s\n",
                   n->ViewportKind(), (int)i, b->role, b->visible ? 1 : 0, b->sx, b->sy, b->width, b->height, b->text.c_str());
        }
        // la BARRA DE ABAJO (toolbar): Mover / Rotar / Escalar... (las ultimas acciones), la tilde y la cruz
        if (n->ToolbarVisible()) {
            n->ToolbarUpdate();
            for (size_t i = 0; i < n->ToolButtons.size(); i++) {
                Button* b = n->ToolButtons[i];
                if (!b->visible) continue;
                printf("@cap herramienta kind=%d i=%d rol=%d x=%d y=%d w=%d h=%d name=%s texto=%s\n",
                       n->ViewportKind(), (int)i, b->role, b->sx, b->sy, b->width, b->height, b->help.c_str(), b->text.c_str());
            }
        }
        // las MANIJAS del gizmo (coords de PANTALLA): 0..2 ejes X/Y/Z, 3..5 planos, 6 centro, 7 anillo de la vista, 8 libre
        if (n->ViewportKind() == 1) {
            for (int k = GizmoAxisX; k <= GizmoFree; k++) {
                float sx = 0, sy = 0;
                if (GizmoHandleInScreen((Viewport3D*)n, k, sx, sy))
                    printf("@cap gizmo manija=%d x=%d y=%d modo=%d\n", k, n->x + (int)(sx + 0.5f), n->y + (int)(sy + 0.5f), GizmoMode());
            }
        }
        // los OBJETOS en la Vista 3D: donde cae el origen de cada uno en pantalla (para apuntarles: cajas, clicks)
        if (n->ViewportKind() == 1) InfoObjects((Viewport3D*)n, SceneCollection);
        if (n->ViewportKind() == 1) {   // el cursor 3D, en pantalla
            float sx = 0, sy = 0;
            if (((Viewport3D*)n)->ProjectPoint(cursor3D.pos, sx, sy))
                printf("@cap cursor3d x=%d y=%d\n", n->x + (int)(sx + 0.5f), n->y + (int)(sy + 0.5f));
        }
        // la LINEA DE TIEMPO: el panel de filas (Summary / objeto / canales), donde cae cada fila y cada keyframe (en el
        // dope sheet, sobre la fila; en el editor de curvas, sobre su curva) y la barra de numeros (para cambiar de
        // cuadro con un click). Para filmarla con el mouse: elegir un keyframe, una curva, borrar una fila
        if (n->ViewportKind() == 5) {
            Timeline* tl = (Timeline*)n;
            tl->BuildDopeRows();
            const bool curves = (tl->mode == Timeline::TL_MODE_CURVES);
            printf("@cap tlpanel x=%d y=%d w=%d ojo=%d curvas=%d numeros=%d f1=%d f11=%d\n", n->x, n->y + tl->stripY, tl->panelW,
                   n->x + tl->DopeEyeX(), curves ? 1 : 0, n->y + tl->numY + tl->barH2 / 2,
                   n->x + (int)(tl->FrameToX(1.0f) + 0.5f), n->x + (int)(tl->FrameToX(11.0f) + 0.5f));
            const int rh = tl->rowH > 0 ? tl->rowH : 1;
            for (size_t r = 0; r < tl->dopeRows.size(); r++) {
                const Timeline::DopeRow& d = tl->dopeRows[r];
                const int ry = n->y + tl->stripY + tl->PosY + (int)r * rh + rh / 2;
                printf("@cap tlfila r=%d y=%d tipo=%d prop=%d comp=%d name=%s\n", (int)r, ry, d.type, d.propId, d.compId,
                       d.name.c_str());
                const AnimProperty* ap = (curves && d.propId >= 0) ? tl->RowCurve(d, false) : NULL;
                for (size_t k = 0; k < d.keys.size(); k++) {
                    const int f = d.keys[k];
                    const int ky = ap ? n->y + (int)(tl->ValueToY(ap->Eval(f, 0.0f)) + 0.5f) : ry;
                    printf("@cap tlkey r=%d f=%d x=%d y=%d\n", (int)r, f, n->x + (int)(tl->FrameToX((float)f) + 0.5f), ky);
                }
            }
        }
        // las FILAS del Outliner (la vista Escena): donde cae el nombre de cada objeto que se ve
        if (n->ViewportKind() == 2 && ((Outliner*)n)->view == OUT_VIEW_SCENE) InfoRows((Outliner*)n, SceneCollection);
        // PROPIEDADES: las pestanias, las tarjetas (su titulo) y las filas, con el centro del valor y del rombo.
        // El MISMO recorrido que Properties::ClickIn (si no, un click del harness cae en otra fila).
        if (n->ViewportKind() == 3) {
            Properties* P = (Properties*)n;
            for (size_t i = 0; i < P->BarTabs.size(); i++) {
                Tab* t = P->BarTabs[i];
                if (t->visible) printf("@cap pestania i=%d x=%d y=%d w=%d h=%d activa=%d\n", (int)i, t->sx, t->sy, t->width, t->height, t->active ? 1 : 0);
            }
            int yCursor = P->y + P->BarTopOffset() + P->PosY + borderGS + RowHeightGS + gapGS;
            for (size_t i = 0; i < P->GroupProperties.size(); i++) {
                GroupPropertie* g = P->GroupProperties[i];
                if (!g->visible) continue;
                const int hCab = borderGS + RowHeightGS + gapGS;
                const int x0 = P->x + P->PosX + borderGS;
                printf("@cap tarjeta i=%d x=%d y=%d w=%d h=%d abierta=%d titulo=%s\n", (int)i, x0, yCursor, g->width, hCab, g->open ? 1 : 0, g->name.c_str());
                if (g->open) {
                    int yRow = yCursor + hCab;
                    const int xRight = P->x + P->PosX + borderGS * 2 + g->width - bordersGS;
                    for (size_t j = 0; j < g->properties.size(); j++) {
                        PropertieBase* prop = g->properties[j];
                        const int hRow = prop->Resize(g->width);
                        if (hRow > 0) {
                            const int endValue   = xRight - (g->keyBtnCol > 0 ? g->keyBtnCol : 0);
                            const int startValue = x0 + g->colLabel;
                            const int rx = (prop->AnimProp() >= 0 && g->keyBtnCol > 0) ? xRight - g->keyBtnCol / 2 : -1;
                            printf("@cap prop g=%d j=%d y=%d h=%d tipo=%d valor=%d rombo=%d name=%s\n", (int)i, (int)j, yRow + hRow / 2, hRow,
                                   (int)prop->GetType(), (startValue + endValue) / 2, rx, prop->name.c_str());
                        }
                        yRow += hRow;
                    }
                }
                yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
            }
            // el OJO de cada script de la lista (tarjeta Control): donde tocar para prenderlo / apagarlo
            if (ActiveObj && ActiveObj->scriptData) {
                for (size_t f = 0; f < ActiveObj->scriptData->scripts.size(); f++) {
                    int ox, oy;
                    if (PropsEyeScriptCenter(P, (int)f, &ox, &oy))
                        printf("@cap ojoscript fila=%d x=%d y=%d apagado=%d\n", (int)f, ox, oy,
                               ActiveObj->scriptData->scripts[f].off ? 1 : 0);
                }
            }
        }
    }

    void InfoMenu(PopupMenu* m, int level) {
        if (!m || !m->open) return;
        printf("@cap menu nivel=%d x=%d y=%d w=%d h=%d filas=%d scroll=%d sel=%d titulo=%s\n",
               level, m->x, m->y, m->width, m->height, (int)m->items.size(), m->scroll, m->selectIndex, m->title.c_str());
        const int row = RowHeightGS + gapGS;
        const int oy = m->title.empty() ? 0 : row;
        const int visible = m->VisibleRows();
        for (int i = 0; i < (int)m->items.size(); i++) {
            const int v = i - m->scroll;
            if (v < 0 || v >= visible) continue;
            const int cy = m->y + borderGS + oy + v * row + RowHeightGS / 2;
            printf("@cap item nivel=%d i=%d id=%d cx=%d cy=%d texto=%s\n",
                   level, i, m->items[i]->id, m->x + m->width / 2, cy, m->items[i]->text.c_str());
        }
        if (m->openSubmenu) InfoMenu(m->openSubmenu, level + 1);
    }
}

// ============================================================================================================
//  CONTROLES de las pruebas de regresion de lo BASICO (tools/pruebas/prueba_basicos_tutoriales.w3s). El duenio,
//  2026-11-09: "sobre estos errores basicos... espero que se hagan pruebas automaticas para que nunca mas ocurran;
//  la gente tiene que poder hacer estos tutoriales en cualquier sistema y en todos conseguir el mismo resultado".
//  Cada error se prueba por la MISMA puerta que usa la persona (clicks, teclas, dedos), no por la funcion de adentro.
// ============================================================================================================
namespace {
    ViewportBase* TypeLeaf(ViewportBase* n, int kind) {
        if (!n) return NULL;
        if (!n->isLeaf()) {
            ViewportBase* a = TypeLeaf(((ViewportColumn*)n)->childA, kind);
            return a ? a : TypeLeaf(((ViewportColumn*)n)->childB, kind);
        }
        return n->ViewportKind() == kind ? n : NULL;
    }
    void ClickIn(int x, int y, int clicks) {
        if (x != gMouseX || y != gMouseY) Moved(0, gMouseButtons, x, y, gMouseX, gMouseY);
        gMouseX = x; gMouseY = y;
        for (int k = 1; k <= clicks; k++) {
            gClicks = k;
            SendButton(0, SDL_BUTTON_LEFT, true, x, y);
            SendButton(0, SDL_BUTTON_LEFT, false, x, y);
        }
        gClicks = 1;
    }
    void KeyEv(SDL_Keycode k, bool down, Uint16 mod) {
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.timestamp = SDL_GetTicks();
        e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.key.keysym.sym = k;
        e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
        e.key.keysym.mod = mod;
        Dispatch(e);
    }
    // como una persona en un teclado: Shift apretado para las mayusculas, la tecla, la letra (TEXTINPUT) y soltar
    void Write(const std::string& s) {
        for (size_t i = 0; i < s.size(); i++) {
            const char ch = s[i];
            const bool caps = (ch >= 'A' && ch <= 'Z');
            const char low = caps ? (char)(ch - 'A' + 'a') : ch;
            if (caps) KeyEv(SDLK_LSHIFT, true, KMOD_LSHIFT);
            KeyEv((SDL_Keycode)low, true, caps ? KMOD_LSHIFT : 0);
            SDL_Event t; memset(&t, 0, sizeof(t)); t.type = SDL_TEXTINPUT; t.text.timestamp = SDL_GetTicks();
            t.text.text[0] = ch; Dispatch(t);
            KeyEv((SDL_Keycode)low, false, caps ? KMOD_LSHIFT : 0);
            if (caps) KeyEv(SDLK_LSHIFT, false, 0);
        }
    }
    std::string Rest(std::istringstream& ss) {
        std::string r; std::getline(ss, r);
        const size_t p = r.find_first_not_of(" \t");
        return p == std::string::npos ? std::string() : r.substr(p);
    }
    // la fila de Propiedades llamada 'nombre' (ya traducido): el centro de su valor. El MISMO recorrido que capinfo
    // y que Properties::ClickIn. false = no esta, o esta fuera de la pantalla
    // las filas que se ven en Propiedades (para el mensaje de una fila que no esta: que hay en su lugar)
    std::string VisibleRows() {
        Properties* P = (Properties*)TypeLeaf(rootViewport, 3);
        if (!P) return " (no hay panel de Propiedades)";
        std::string r = " (se ven:";
        for (size_t i = 0; i < P->GroupProperties.size(); i++) {
            GroupPropertie* g = P->GroupProperties[i];
            if (!g->visible) continue;
            r += " [" + g->name + "]";
            if (!g->open) { r += " cerrada"; continue; }
            for (size_t j = 0; j < g->properties.size(); j++)
                if (g->properties[j]->Resize(g->width) > 0) r += " '" + g->properties[j]->name + "'";
        }
        return r + ")";
    }
    bool RowProperties(const std::string& name, int& x, int& y, PropertieBase** out) {
        Properties* P = (Properties*)TypeLeaf(rootViewport, 3);
        if (!P) return false;
        int yCursor = P->y + P->BarTopOffset() + P->PosY + borderGS + RowHeightGS + gapGS;
        for (size_t i = 0; i < P->GroupProperties.size(); i++) {
            GroupPropertie* g = P->GroupProperties[i];
            if (!g->visible) continue;
            const int hCab = borderGS + RowHeightGS + gapGS;
            const int x0 = P->x + P->PosX + borderGS;
            if (g->open) {
                int yRow = yCursor + hCab;
                const int xRight = P->x + P->PosX + borderGS * 2 + g->width - bordersGS;
                for (size_t j = 0; j < g->properties.size(); j++) {
                    PropertieBase* prop = g->properties[j];
                    const int hRow = prop->Resize(g->width);
                    if (hRow > 0 && prop->name == name) {
                        const int endValue = xRight - (g->keyBtnCol > 0 ? g->keyBtnCol : 0);
                        x = (x0 + g->colLabel + endValue) / 2; y = yRow + hRow / 2;
                        if (out) *out = prop;
                        return y >= P->y && y < P->y + P->height;
                    }
                    yRow += hRow;
                }
            }
            yCursor += g->height + borderGS + (g->open ? GlobalScale : 0);
        }
        return false;
    }
    bool RowOutlinerOf(Outliner* o, Object* root, const std::string& name, int& x, int& y) {
        if (!root) return false;
        for (size_t i = 0; i < root->Childrens.size(); i++) {
            Object* ob = root->Childrens[i];
            const int row = OutlinerObjectRow(ob);
            if (row >= 0 && ob->name == name) {
                int prof = 0;
                for (Object* p = ob->Parent; p && p != SceneCollection; p = p->Parent) prof++;
                x = o->x + marginGS + o->PosX + (prof + 2) * (IconSizeGS + gapGS) + LetterWidthGS;
                y = o->y + borderGS + o->PosY + o->BarTopOffset() + OutlinerYOfRow(row) + (int)RowHeightGS / 2;
                return true;
            }
            if (RowOutlinerOf(o, ob, name, x, y)) return true;
        }
        return false;
    }
    Mesh* MeshCall(const std::string& name) {
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
        return (o && o->getType() == ObjectType::mesh) ? (Mesh*)o : NULL;
    }
}

bool W3dCapturesCmd(const std::string& cmd, std::istringstream& ss, std::string& err, bool& handled) {
    handled = true;
    if (cmd == "mouse") {
        std::string that, which; int x = 0, y = 0; ss >> that >> x >> y >> which;
        const int b = ButtonOf(which);
        if (that == "move") {
            Moved(0, gMouseButtons, x, y, gMouseX, gMouseY);
        } else if (that == "down" || that == "up") {
            if (x != gMouseX || y != gMouseY) Moved(0, gMouseButtons, x, y, gMouseX, gMouseY);
            const bool down = (that == "down");
            if (down) gMouseButtons |= SDL_BUTTON(b); else gMouseButtons &= ~SDL_BUTTON(b);
            SendButton(0, b, down, x, y);
        } else if (that == "double") {   // doble click: dos clicks seguidos, el segundo con clicks = 2
            if (x != gMouseX || y != gMouseY) Moved(0, gMouseButtons, x, y, gMouseX, gMouseY);
            for (int k = 1; k <= 2; k++) {
                gClicks = k;
                gMouseButtons |= SDL_BUTTON(b); SendButton(0, b, true, x, y);
                gMouseButtons &= ~SDL_BUTTON(b); SendButton(0, b, false, x, y);
            }
            gClicks = 1;
        } else { err = "mouse: uso: mouse down|move|up|doble <x> <y> [izq|medio|der]"; return false; }
        gMouseX = x; gMouseY = y;
        return true;
    }
    // 'eyescript <fila>': un click de verdad en el OJO de ese script (la lista de la tarjeta Control, en Propiedades)
    // 'scriptoff <fila> 0|1': el script de esa fila del objeto activo esta apagado (1) o prendido (0)
    // 'saveasfill <nombre> <interno 0|1>': llena el "Guardar como..." abierto (el nombre y la casilla "Dentro del
    // .w3d") SIN guardar: para la captura del formulario (guardarcomo, en W3dScript.cpp, lo llena y guarda)
    if (cmd == "saveasfill") {
        std::string name; int internal = -1; ss >> name >> internal;
        if (!SaveAsOpen()) { err = "saveasfill: el formulario no esta abierto"; return false; }
        SaveAsSet(name, internal, std::string(), std::string());
        g_redraw = true; Frame();
        return true;
    }
    // 'scriptadd': el boton Aniadir de la tarjeta Control (objeto activo): abre su menu (los scripts del proyecto y
    // "Cargar .lua..."); despues se elige con menuelegir <nombre>
    if (cmd == "scriptadd") {
        if (!ActiveObj) { err = "scriptadd: no hay objeto activo"; return false; }
        extern void _ScriptAddFwd(); _ScriptAddFwd();
        g_redraw = true; Frame();
        return true;
    }
    if (cmd == "eyescript" || cmd == "scriptoff") {
        int row = -1, expected = -1; ss >> row >> expected;
        if (!ActiveObj || !ActiveObj->scriptData || row < 0 || row >= (int)ActiveObj->scriptData->scripts.size()) {
            err = cmd + ": el objeto activo no tiene el script " + std::to_string(row); return false;
        }
        if (cmd == "scriptoff") {
            const int ap = ActiveObj->scriptData->scripts[row].off ? 1 : 0;
            printf("      [scriptapagado] fila %d: %s\n", row, ap ? "APAGADO" : "on");
            if (expected >= 0 && ap != expected) { err = "scriptoff: se esperaba " + std::to_string(expected); return false; }
            return true;
        }
        Properties* P = NULL;
        struct VK { static ViewportBase* Find(ViewportBase* n) { if (!n) return NULL; if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
                        ViewportBase* a = Find(((ViewportColumn*)n)->childA); if (a) return a; return Find(((ViewportColumn*)n)->childB); } };
        P = (Properties*)VK::Find(rootViewport);
        int x = 0, y = 0;
        if (!P || !PropsEyeScriptCenter(P, row, &x, &y)) { err = "eyescript: no se ve el ojo de la fila " + std::to_string(row); return false; }
        Moved(0, gMouseButtons, x, y, gMouseX, gMouseY);
        gMouseButtons |= SDL_BUTTON(1); SendButton(0, 1, true, x, y);
        gMouseButtons &= ~SDL_BUTTON(1); SendButton(0, 1, false, x, y);
        gMouseX = x; gMouseY = y;
        return true;
    }
    // 'help [texto en ingles]': el cartelito de ayuda (Button::ayuda, el que muestra W3dHelpRender despues de un
    // momento quieto) de lo que esta bajo el mouse en Propiedades; con texto, falla si no es ese. (El cartelito en si no
    // sale en las capturas: el harness no dibuja la capa de los avisos)
    if (cmd == "help") {
        std::string expected; std::getline(ss, expected);
        const size_t p0 = expected.find_first_not_of(" \t"); expected = (p0 == std::string::npos) ? "" : expected.substr(p0);
        struct VK { static ViewportBase* Find(ViewportBase* n) { if (!n) return NULL; if (n->isLeaf()) return n->ViewportKind() == 3 ? n : NULL;
                        ViewportBase* a = Find(((ViewportColumn*)n)->childA); if (a) return a; return Find(((ViewportColumn*)n)->childB); } };
        Properties* P = (Properties*)VK::Find(rootViewport);
        Button* b = P ? PropsHelpIn(P, gMouseX, gMouseY) : NULL;
        const std::string has = b ? b->help : std::string();
        printf("      [ayuda] (%d, %d): '%s'\n", gMouseX, gMouseY, has.c_str());
        if (!expected.empty() && has != expected) { err = "help: se esperaba '" + expected + "'"; return false; }
        return true;
    }
    // 'tbtap <rol>': un toque con el DEDO en el boton de la barra de herramientas de la Vista 3D que tenga ese rol
    // (accept, cancel, individual, boundary, outset, depth, segless, segmore, vertices): el tilde, la cruz y las
    // opciones de la operacion en curso, por el mismo camino que un dedo de verdad
    if (cmd == "tbtap") {
        std::string r; ss >> r;
        const int role = r == "accept" ? TBR_Accept : r == "cancel" ? TBR_Cancel : r == "individual" ? TBR_InsetIndividual :
                         r == "boundary" ? TBR_InsetBoundary : r == "outset" ? TBR_InsetOutset : r == "depth" ? TBR_InsetDepth :
                         r == "segless" ? TBR_BevelSegLess : r == "segmore" ? TBR_BevelSegMore : r == "vertices" ? TBR_BevelVertices : -1;
        Viewport3D* v3 = (Viewport3D*)TypeLeaf(rootViewport, 1);
        if (role < 0 || !v3) { err = "tbtap: uso: tbtap accept|cancel|individual|boundary|outset|depth|segless|segmore|vertices"; return false; }
        Frame();   // (la barra se arma al dibujar)
        Button* b = BarRoleBtn(v3->ToolButtons, role);
        if (!b || !b->visible) { err = "tbtap: el boton '" + r + "' no esta en la barra"; return false; }
        const int x = b->sx + b->width / 2, y = b->sy + b->height / 2;
        Moved(SDL_TOUCH_MOUSEID, 0, x, y, x, y);
        SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y);  SendFinger(SDL_FINGERDOWN, 1, x, y, x, y);
        SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, x, y); SendFinger(SDL_FINGERUP, 1, x, y, x, y);
        Frame();
        return true;
    }
    if (cmd == "finger") {
        std::string that; int x = -1, y = -1, id = 1; ss >> that >> x >> y; if (!(ss >> id)) id = 1;
        // 'finger up' sin lugar: donde esta ese dedo (el que apoyo 'propfinger', por ejemplo)
        if (x < 0 && y < 0 && that == "up" && gFingers.count(id)) { x = gFingers[id].first; y = gFingers[id].second; }
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        // el orden de SDL: primero el mouse sintetizado (which = SDL_TOUCH_MOUSEID), despues el del dedo. SDL
        // sintetiza el mouse solo con el PRIMER dedo (id 1): los demas (dos dedos = zoom y paneo) son solo dedo
        const bool first = (id == 1);
        std::map<int, std::pair<int, int> >::iterator ant = gFingers.find(id);
        const int px = (ant != gFingers.end()) ? ant->second.first : x;
        const int py = (ant != gFingers.end()) ? ant->second.second : y;
        if (that == "down") {
            if (first) { Moved(SDL_TOUCH_MOUSEID, 0, x, y, x, y); SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y); }
            SendFinger(SDL_FINGERDOWN, id, x, y, x, y);
        } else if (that == "move") {
            if (first) Moved(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, x, y, px, py);
            SendFinger(SDL_FINGERMOTION, id, x, y, px, py);
        } else if (that == "up") {
            if (first) SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, x, y);
            SendFinger(SDL_FINGERUP, id, x, y, px, py);
        } else { err = "finger: uso: finger down|move|up <x> <y> [id]"; return false; }
        if (that == "up") gFingers.erase(id); else gFingers[id] = std::make_pair(x, y);
        return true;
    }
    // 'fingertouch esquina <tipo>' / 'fingertouch item <opcion en ingles>': un TOQUE con el dedo (bajar y levantar, como
    // 'finger') en el boton de la esquina del primer viewport de ese tipo, o en una opcion del menu abierto
    // 'menuadd <x> <y>': abre el menu Anadir (el MISMO de la barra del 3D) en ese lugar, como Shift+A
    if (cmd == "menuadd") {
        int x = 0, y = 0; ss >> x >> y;
        extern void LayoutMenuAdd(int, int); LayoutMenuAdd(x, y);
        extern PopupMenu* OpenMenu;
        printf("      [menuadd] abierto en (%d, %d) ancho %d (pantalla %d)\n", OpenMenu ? OpenMenu->x : -1,
               OpenMenu ? OpenMenu->y : -1, OpenMenu ? OpenMenu->width : -1, MenuScreenW);
        g_redraw = true; Frame();
        return true;
    }
    if (cmd == "fingertouch") {
        std::string that; ss >> that;
        int x = -1, y = -1;
        if (that == "corner") {
            int kind = 0; ss >> kind;
            ViewportBase* v = TypeLeaf(rootViewport, kind);
            if (v) { v->UpdateBar(); if (!v->BarButtons.empty()) { Button* b = v->BarButtons[0]; x = b->sx + b->width / 2; y = b->sy + b->height / 2; } }
            if (x < 0) { err = "fingertouch: no hay un viewport de tipo " + std::to_string(kind) + " con su esquina"; return false; }
        } else if (that == "button") {   // 'fingertouch boton <icono> [tipo]': el boton de la barra con ese icono ("mas" = Anadir)
            std::string ic; int kind = 1; ss >> ic; if (!(ss >> kind)) kind = 1;
            ViewportBase* v = TypeLeaf(rootViewport, kind);
            const int idx = IconIndex(ic.c_str());
            if (v) { v->UpdateBar();
                     for (size_t i = 0; i < v->BarButtons.size() && x < 0; i++)
                         if (v->BarButtons[i] && v->BarButtons[i]->icon == idx) { Button* b = v->BarButtons[i]; x = b->sx + b->width / 2; y = b->sy + b->height / 2; } }
            if (x < 0) { err = "fingertouch: no hay un boton '" + ic + "' en la barra del viewport de tipo " + std::to_string(kind); return false; }
        } else if (that == "item" || that == "itemleft" || that == "subitem") {   // itemizq: sobre el TEXTO (a la izquierda), como
            const std::string t = T(Rest(ss).c_str());                         // el dedo de verdad; subitem: en el submenu
            extern PopupMenu* OpenMenu;                                        // abierto mas hondo
            PopupMenu* m = (OpenMenu && OpenMenu->open) ? OpenMenu : NULL;
            if (that == "subitem") while (m && m->openSubmenu && m->openSubmenu->open) m = m->openSubmenu;
            const int row = RowHeightGS + gapGS;
            if (m) for (int i = 0; i < (int)m->items.size(); i++) {
                const int v = i - m->scroll;
                if (m->items[i]->text != t || v < 0 || v >= m->VisibleRows()) continue;
                x = (that == "itemleft") ? m->x + IconSizeGS * 3 : m->x + m->width / 2;
                y = m->y + borderGS + (m->title.empty() ? 0 : row) + v * row + RowHeightGS / 2;
            }
            if (x < 0) { err = "fingertouch: el menu abierto no tiene '" + t + "' a la vista"; return false; }
        } else { err = "fingertouch: uso: fingertouch esquina <tipo> | dedotoque boton <icono> [tipo] | dedotoque item|itemizq <opcion>"; return false; }
        printf("      [dedotoque] %s en (%d, %d)\n", that.c_str(), x, y);
        Moved(SDL_TOUCH_MOUSEID, 0, x, y, x, y); SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y);
        SendFinger(SDL_FINGERDOWN, 1, x, y, x, y);
        Frame();   // (un toque de verdad dura algunos cuadros: el editor dibuja con el dedo apoyado)
        SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, x, y);
        SendFinger(SDL_FINGERUP, 1, x, y, x, y);
        gFingers.erase(1);
        g_redraw = true; Frame();
        return true;
    }
    // 'vptype <tipo> <id>': el primer viewport de ese tipo pasa a ser otro, por la misma puerta que el menu de tipos (ids
    // del menu: 0 Vista 3D, 1 Outliner, 2 Propiedades, 3 Editor UV, 4 Linea de tiempo, 5 2D, 6 Consola, 7 IDE, 8 Biblioteca)
    if (cmd == "vptype") {
        int kind = 0, id = 0; ss >> kind >> id;
        ViewportBase* v = TypeLeaf(rootViewport, kind);
        if (!v) { err = "vptype: no hay un viewport de tipo " + std::to_string(kind); return false; }
        extern void LayoutChangeViewportType(ViewportBase* v, int aId);
        LayoutChangeViewportType(v, id);
        g_redraw = true; Frame();
        return true;
    }
    // 'root <tipo>': el viewport que ocupa toda la ventana es de ese tipo (1 = 3D, 5 = linea de tiempo...; 0 = el layout
    // entero, sin nada maximizado)
    if (cmd == "root") {
        int kind = 0; ss >> kind;
        const int has = (rootViewport && rootViewport->isLeaf()) ? rootViewport->ViewportKind() : 0;
        printf("      [raiz] %d\n", has);
        if (has != kind) { err = "root: es de tipo " + std::to_string(has) + ", se esperaba " + std::to_string(kind); return false; }
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
    if (cmd == "wheel") {
        int steps = 0, x = gMouseX, y = gMouseY; ss >> steps >> x >> y;
        if (steps == 0) { err = "wheel: uso: wheel <pasos> <x> <y> (positivo = arriba)"; return false; }
        Wheel(steps, x, y);
        return true;
    }
    if (cmd == "key") {
        std::string name, as; ss >> name >> as;
        for (size_t i = 0; i < name.size(); i++) if (name[i] == '_') name[i] = ' ';   // "Keypad_7" = "Keypad 7"
        const SDL_Keycode k = SDL_GetKeyFromName(name.c_str());
        if (k == SDLK_UNKNOWN) { err = "key: SDL no conoce '" + name + "'"; return false; }
        for (int step = 0; step < 2; step++) {
            const bool down = (step == 0);
            if ((as == "down" && !down) || (as == "up" && down)) continue;
            SDL_Event e; memset(&e, 0, sizeof(e));
            e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.timestamp = SDL_GetTicks();
            e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
            e.key.keysym.sym = k;
            e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
            e.key.keysym.mod = (Uint16)SDL_GetModState();   // los que dejo 'mod' (Ctrl + A = 'mod ctrl' y 'key a')
            Dispatch(e);
        }
        return true;
    }
    if (cmd == "drawframe") { Frame(); return true; }
    // 'modemenu <objeto|edicion|pose|vertices|pesos|textura>': el modo del objeto activo, por la MISMA puerta que el menu
    // del boton de modo (LayoutModePick). El 'mode' de las pruebas no tiene el Modo Pose.
    if (cmd == "modemenu") {
        std::string q; ss >> q;
        const int m = (q == "object") ? ObjectMode : (q == "edit") ? EditMode : (q == "pose") ? PoseMode :
                      (q == "vertices") ? VertexPaint : (q == "weights") ? WeightPaint : (q == "texture") ? TexturePaint : -1;
        if (m < 0) { err = "modemenu: objeto|edicion|pose|vertices|pesos|textura"; return false; }
        extern void LayoutModePick(int mode); LayoutModePick(m);
        if (InteractionMode != m) { err = "modemenu: el objeto activo no tiene ese modo"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'outliner n95 mover|visible|render': las teclas 1 / 2 / 3 del N95 con el Outliner activo (las MISMAS llamadas que
    // platform/symbian/src/w3dlayout.cpp: W3dOutlinerMoveToggle / ToggleVisible / ToggleRender).
    // 'outliner tecla <Up|Down|Left|Right|Enter|Cancel>': una tecla del N95 al panel activo (LayoutKeyPanelActive).
    if (cmd == "outliner") {
        std::string that, which; ss >> that >> which;
        if (!viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 2) { err = "outliner: el viewport activo no es un Outliner"; return false; }
        Outliner* out = (Outliner*)viewPortActive;
        if (that == "n95") {
            if (which == "move") {
                if (out->ModeMove()) out->MoveConfirm();
                else if (out->movingRec) out->MoveRecConfirm();
                else out->MoveStart();
            } else if (which == "visible") { extern void ChangeVisibilityObj(); ChangeVisibilityObj(); }
            else if (which == "render") { extern void ChangeRenderableObj(); ChangeRenderableObj(); }
            else { err = "outliner n95: mover|visible|render"; return false; }
        } else if (that == "key") {   // (LayoutKey, LayoutInput.h: los codigos propios del panel, no los de SDL)
            const int k = (which == "Up") ? 0 : (which == "Down") ? 1 : (which == "Left") ? 2 : (which == "Right") ? 3 :
                          (which == "Enter") ? 4 : (which == "Cancel") ? 5 : -1;
            if (k < 0) { err = "outliner tecla: Up|Down|Left|Right|Enter|Cancel"; return false; }
            { extern bool LayoutKeyPanelActive(int key); LayoutKeyPanelActive(k); }
        } else { err = "outliner: uso: outliner n95 <mover|visible|render> | outliner tecla <tecla>"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'tick': el reloj de los controles (ControlsTick: las pulsaciones largas tactiles), como en cada cuadro del editor
    if (cmd == "tick") { extern void ControlsTick(); ControlsTick(); g_redraw = true; Frame(); return true; }
    // 'menuopen si|no': que haya (o no) un menu desplegado ahora (la pulsacion larga abre el contextual)
    if (cmd == "menuopen") {
        std::string q; ss >> q;
        extern PopupMenu* OpenMenu;
        const bool has = OpenMenu && OpenMenu->open;
        printf("      [menuabierto] %s%s%s\n", has ? "si" : "no", has ? ": " : "", has ? OpenMenu->title.c_str() : "");
        if ((q == "si") != has) { err = std::string("menuopen: esperaba ") + q; return false; }
        return true;
    }
    // 'text <caracteres>': lo tipeado (SDL_TEXTINPUT, como el teclado de verdad despues de la tecla): los numeros de
    // un transform ("R Z 90"), un nombre... Las teclas sueltas ('key') no escriben: solo mandan la tecla.
    if (cmd == "text") {
        std::string t; ss >> t;
        if (t.empty()) { err = "text: uso: text <caracteres>"; return false; }
        SDL_Event e; memset(&e, 0, sizeof(e));
        e.type = SDL_TEXTINPUT;
        e.text.timestamp = SDL_GetTicks();
        strncpy(e.text.text, t.c_str(), sizeof(e.text.text) - 1);
        Dispatch(e);
        return true;
    }
    // 'origin cursor|geometria': Objeto > Definir Origen > Origen al Cursor 3D / Origen a Geometria, sin pasar por el
    // menu (para preparar una escena: en el N95 la barra de menus no entra entera en la pantalla)
    if (cmd == "origin") {
        std::string that; ss >> that;
        extern void SetOriginToCursor(); extern void SetOriginOriginToGeometry();
        if (that == "cursor") SetOriginToCursor();
        else if (that == "geometry") SetOriginOriginToGeometry();
        else { err = "origin: uso: origin cursor|geometria"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'pivot <0..3>': el punto de pivote (TransformPivot: 0 Cursor 3D, 1 Origenes individuales, 2 Punto medio,
    // 3 Elemento activo). El boton de la barra de la Vista 3D muestra su icono.
    if (cmd == "pivot") {
        int v = -1; ss >> v;
        if (v < 0 || v > 3) { err = "pivot: uso: pivot <0..3>"; return false; }
        g_transformPivot = v;
        g_redraw = true; Frame();
        return true;
    }
    // 'cursor3d <x> <y> <z>': lleva el cursor 3D a ese punto (como el iman, sin pasar por el menu)
    if (cmd == "cursor3d") {
        float x = 0, y = 0, z = 0;
        if (!(ss >> x >> y >> z)) { err = "cursor3d: uso: cursor3d <x> <y> <z>"; return false; }
        cursor3D.pos = Vector3(x, y, z);
        g_redraw = true; Frame();
        return true;
    }
    // 'addui sonido': Anadir > Sonido (en el cursor 3D, elegido). 'addui restriccion <0..3>': la restriccion de ese
    // tipo (W3dConstraintType) en el objeto activo, por la puerta del boton Anadir de Propiedades. 'addui
    // modificador <n>': el modificador de ese ModifierType en la malla activa (tambien Esqueleto, que 'modadd' no tiene).
    if (cmd == "addui") {
        std::string that; ss >> that;
        if (that == "sound") { extern void AddSound(); AddSound(); }
        else if (that == "empty") { extern void AddEmpty(); AddEmpty(); }   // Aniadir > Vacio (en el cursor 3D)
        else if (that == "reflectionprobe") { extern void AddReflectionProbe(); AddReflectionProbe(); }   // Aniadir > Sonda de reflejo
        else if (that == "constraint" || that == "modifier") {
            int t = -1; ss >> t;
            if (!ActiveObj || t < 0) { err = "addui: sin objeto activo o sin tipo"; return false; }
            if (that == "constraint") UndoConstraintAdd(ActiveObj, t);
            else {
                if (ActiveObj->getType() != ObjectType::mesh) { err = "addui modificador: el activo no es una malla"; return false; }
                UndoModAdd((Mesh*)ActiveObj, t);
                ((Mesh*)ActiveObj)->GenerateMeshModified();
            }
        } else { err = "addui: uso: addui sonido | vacio | sondareflejo | restriccion <tipo> | modificador <tipo>"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'view3d sombreado <n>': el modo de la Vista 3D activa (RenderType: 0 Solido, 1 Vista previa de material,
    // 2 Renderizado...). 'view3d huesos <0|1>': dibujar o no los esqueletos (Superposiciones > Objetos).
    // 'view3d gizmo <0|1>': Superposiciones > Gizmo (el N95 arranca sin gizmo). 'view3d barra <0|1>': la barra de
    // abajo (en el N95 viene apagada: cfg.newUser).
    if (cmd == "view3d") {
        std::string that; int v = -1; ss >> that >> v;
        if (!Viewport3DActive && rootViewport) rootViewport->Render();
        Viewport3D* vp = Viewport3DActive;
        if (!vp) { err = "view3d: no hay viewport 3D"; return false; }
        if (that == "shading" && v >= 0 && v <= (int)RenderType::Vectors) vp->view.v = (RenderType::Enum)v;
        else if (that == "bones" && (v == 0 || v == 1)) vp->showArmature = (v == 1);
        else if (that == "gizmo" && (v == 0 || v == 1)) g_gizmoOn = (v == 1);   // (en el N95 arranca apagado)
        else if (that == "bar" && (v == 0 || v == 1)) { extern bool g_captureNoBar3D; g_captureNoBar3D = (v == 0); }
        else if (that == "overlays" && (v == 0 || v == 1)) vp->showOverlays = (v == 1);   // Superposiciones > Mostrar
        // 'view3d trazado <0|1> [pases]': la vista Render con el TRAZADO DE RAYOS progresivo (el de Propiedades > Render);
        // despues 'tick N' lo deja juntar pases (las capturas de la tienda: la imagen trazada adentro del editor)
        else if (that == "raytraced" && (v == 0 || v == 1)) {
            int passes = 0; ss >> passes;
            g_rt.on = (v == 1);
            if (passes > 0) g_rt.passes = passes;
            if (g_rt.on) vp->view = RenderType::Rendered;
            RTInvalidate();
        }
        else { err = "view3d: uso: view3d sombreado <n> | vista3d huesos <0|1> | vista3d gizmo <0|1> | vista3d barra <0|1> | vista3d overlays <0|1> | vista3d trazado <0|1> [pases]"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'textures': carga YA las texturas que el import dejo en la cola (el editor carga una por cuadro: en una foto
    // sacada enseguida, un modelo recien importado se veria sin textura)
    if (cmd == "textures") { LoadAllPendingTextures(); g_redraw = true; Frame(); return true; }
    // la escala de la interfaz, como la de Ajustes (1 = como se ve en el N95)
    if (cmd == "scale") {
        int v = 0; ss >> v;
        if (v < 1 || v > 6) { err = "scale: uso: scale <1..6>"; return false; }
        SetGlobalScale(v);
        if (rootViewport) rootViewport->Resize(MenuScreenW, MenuScreenH);
        Frame();
        return true;
    }
    // el layout por defecto PARA EL TAMANIO ACTUAL, como si el editor hubiera arrancado asi (el harness abre con
    // otro tamanio y 'window' solo lo redimensiona): en 320x240 son 2 viewports, la Vista 3D y Propiedades, como
    // en el N95. El arbol viejo no se borra (algun puntero del input podria seguir mirandolo; el harness dura poco).
    if (cmd == "layoutdefault") {
        Viewport3D* vp3d = NULL;
        rootViewport = LayoutDefault(MenuScreenW, MenuScreenH, &vp3d);
        viewPortActive = vp3d;
        Viewport3DActive = vp3d;
        rootViewport->Resize(MenuScreenW, MenuScreenH);
        g_redraw = true;
        Frame();
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
        rootViewport->Resize(MenuScreenW, MenuScreenH);
        Frame();
        return true;
    }
    // 'divisionof <kind> <0.05..0.95>': la division que contiene a la hoja de ese tipo (5 = la Linea de tiempo): cuanto
    // ocupa su PRIMER hijo. Para que la Linea de tiempo se vea grande en los tutoriales de animacion
    if (cmd == "divisionof") {
        int k = -1; float f = 0; ss >> k >> f;
        if (f < 0.05f || f > 0.95f) { err = "divisionof: uso: divisionof <kind> <0.05..0.95>"; return false; }
        struct B {
            static ViewportBase* Parent(ViewportBase* n, int kind) {
                if (!n || n->isLeaf()) return NULL;
                ViewportColumn* c = (ViewportColumn*)n;
                if ((c->childA && c->childA->isLeaf() && c->childA->ViewportKind() == kind) ||
                    (c->childB && c->childB->isLeaf() && c->childB->ViewportKind() == kind)) return n;
                ViewportBase* r = Parent(c->childA, kind);
                return r ? r : Parent(c->childB, kind);
            }
        };
        ViewportBase* p = B::Parent(rootViewport, k);
        if (!p) { err = "divisionof: no hay una division con una hoja de ese tipo"; return false; }
        if (p->ContainerKind() == 1) ((ViewportRow*)p)->splitFrac = f;
        else                         ((ViewportColumn*)p)->splitFrac = f;
        rootViewport->Resize(MenuScreenW, MenuScreenH);
        Frame();
        return true;
    }
    // la tecla VERDE del N95: 'green' = un toque (pasa al viewport siguiente); 'green <dx> <dy> [cuadros]' = verde
    // mantenida + una flecha durante esos cuadros (el telefono redimensiona un paso de 0.01 por cuadro)
    if (cmd == "green") {
        int dx = 0, dy = 0, frames = 1;
        if (!(ss >> dx >> dy)) { LayoutCycleActiveViewport(1); g_redraw = true; Frame(); return true; }
        if (!(ss >> frames)) frames = 1;
        for (int i = 0; i < frames; i++) { LayoutResizeActiveViewport(dx, dy, 0.01f); g_redraw = true; Frame(); }
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
        std::string that; ss >> that;
        if (!Viewport3DActive && rootViewport) rootViewport->Render();   // (lo puebla el dibujado)
        Viewport3D* vp = Viewport3DActive;
        if (!vp) { err = "n95: no hay viewport 3D"; return false; }
        int a = 0, b = 0, frames = 1;
        if (that == "orbit" || that == "pan") {
            if (!(ss >> a >> b)) { err = "n95: uso: n95 " + that + " <dx> <dy> [cuadros]"; return false; }
            if (!(ss >> frames)) frames = 1;
            for (int i = 0; i < frames; i++) {
                if (that == "orbit") vp->OrbitArrow(a * 8, b * 8);
                else                  vp->PanArrow(-a * 8, -b * 8);
                g_redraw = true; Frame();
            }
        } else if (that == "zoom") {
            if (!(ss >> b)) { err = "n95: uso: n95 zoom <dy> [cuadros]"; return false; }
            if (!(ss >> frames)) frames = 1;
            for (int i = 0; i < frames; i++) { vp->Zoom((float)(-b)); g_redraw = true; Frame(); }
        } else if (that == "view") {
            if (!(ss >> a >> b)) { err = "n95: uso: n95 vista <dx> <dy>"; return false; }
            vp->QuadrantViewNav(a, b);
        } else if (that == "perspective") vp->ChangePerspective();
        else if (that == "focus") vp->FocusObject();
        else if (that == "camera") vp->SetViewFromCameraActive(!vp->ViewFromCameraActive);
        else if (that == "lockorbit") { extern bool LayoutLockOrbitToggle(); LayoutLockOrbitToggle(); }
        // MOVER / ROTAR / ESCALAR con el keypad (platform/symbian/src/w3dnewscene.cpp, W3dNewTransform*), en Modo Objeto:
        //   n95 transformar <1|2|3>              1 / 2 / 3 sin transform en curso: mover / rotar / escalar
        //   n95 eje <0|1|2>                      1 / 2 / 3 durante el transform: eje X / Y / Z (otra vez: cicla la orientacion)
        //   n95 transflechas <dx> <dy> [cuadros] las flechas mantenidas (mover: 4 por cuadro; rotar y escalar: 8)
        //   n95 aceptar | n95 cancelar           OK (el centro del pad) / C
        else if (that == "transform") {
            int m = 0; ss >> m;
            if (m < 1 || m > 3) { err = "n95: uso: n95 transformar <1|2|3>"; return false; }
            // MODO EDICION: como W3dNewTransformStart del telefono (platform/symbian/src/w3dnewscene.cpp): el transform
            // es de los vertices, aristas o caras elegidos (el EditXform compartido), no del objeto entero
            if (InteractionMode == EditMode && g_editMesh) {
                extern void EditXformSnapshot(); extern bool EditXformActive();
                if (m == 1)      { state = translation; axisSelect = ViewAxis; }
                else if (m == 2) { state = rotation;    axisSelect = ViewAxis; }
                else             { state = EditScale;   axisSelect = XYZ; }
                EditXformSnapshot();
                if (!EditXformActive()) { state = editNavigation; err = "n95 transformar: nada elegido en el Modo Edicion"; return false; }
                gModeN95 = m;
                g_redraw = true; Frame();
                return true;
            }
            if (!ActiveObj || !ActiveObj->select) { err = "n95 transformar: no hay un objeto activo seleccionado"; return false; }
            if (m == 1) BeginTranslate();
            else {
                if (!saveState()) { err = "n95 transformar: no se pudo guardar el estado"; return false; }
                state = (m == 2) ? rotation : EditScale;
                axisSelect = ViewAxis;
            }
            gModeN95 = m;
        } else if (that == "extrude") {   // la 7 del N95 en el Modo Edicion: extruye y queda moviendo (el lapiz + el numero)
            extern void LayoutExtrudeFaces(); LayoutExtrudeFaces();
            if (state == editNavigation) { err = "n95 extruir: no arranco (hay caras elegidas en el Modo Edicion?)"; return false; }
            gModeN95 = 1;
        } else if (that == "axis") {
            int e = 0; ss >> e;
            if (!gModeN95) { err = "n95 eje: no hay un transform en curso"; return false; }
            CycleAxisTransform(e);
        } else if (that == "transarrows") {
            if (!gModeN95) { err = "n95 transflechas: no hay un transform en curso"; return false; }
            if (!(ss >> a >> b)) { err = "n95: uso: n95 transflechas <dx> <dy> [cuadros]"; return false; }
            if (!(ss >> frames)) frames = 1;
            const int d = (gModeN95 == 1) ? 4 : 8;
            for (int i = 0; i < frames; i++) {
                if (gModeN95 == 1) SetTranslationObjects(a * d, b * d, vp->DragVelocityWorld());
                else if (gModeN95 == 2) SetRotation(a * d, b * d);
                else SetScale(a * d, b * d, 0.001f);
                g_redraw = true; Frame();
            }
        } else if (that == "accept" || that == "cancel") {
            if (!gModeN95) { err = "n95 " + that + ": no hay un transform en curso"; return false; }
            if (that == "accept") vp->Accept(); else { Cancel(); state = editNavigation; }
            gModeN95 = 0;
        }
        else { err = "n95: accion desconocida '" + that + "' (orbitar|zoom|panear|vista|perspectiva|encuadrar|camara|bloquearorbita|transformar|eje|transflechas|aceptar|cancelar)"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'faces <n> <n> ...': en Modo Edicion, ELEGIR esas caras (solo esas, todas juntas). El 'select face' de los
    // tests deja una sola: cada una reemplaza a la anterior
    if (cmd == "faces") {
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "faces: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e) { err = "faces: la malla no tiene edit mesh"; return false; }
        EditSelectMode = SelFace;
        e->SelectAll(false);
        int f = 0, n = 0;
        while (ss >> f) {
            if (f < 0 || f >= e->NumFaces()) { err = "faces: cara fuera de rango"; return false; }
            e->ToggleFace(f, false);
            n++;
        }
        if (!n) { err = "faces: uso: faces <n> <n> ..."; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'stl exportar <ruta> [sel]': ExportSTL (todas las mallas o las seleccionadas), igual que Propiedades > Exportar.
    // 'stl revisar <malla> <puntos> <triangulos> <ymin> <ymax>': la malla (por nombre) tiene esos puntos UNICOS (los
    // vertices soldados), esos triangulos y esa altura en Y (en local): asi se ve que el STL entro unido y parado.
    // 'stl archivo <ruta> <triangulos> <zmax>': el .stl tiene esos triangulos y su Z mas alta es esa (Z arriba).
    if (cmd == "stl") {
        std::string that; ss >> that;
        if (that == "export") {
            std::string path, sel; ss >> path >> sel;
            if (!ExportSTL(path, sel == "sel", true, true)) { err = "stl exportar: fallo"; return false; }
            return true;
        }
        if (that == "check") {
            std::string name; int points = -1, tris = -1; float ymin = 0, ymax = 0; ss >> name >> points >> tris >> ymin >> ymax;
            Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
            if (!o || o->getType() != ObjectType::mesh) { err = "stl revisar: no hay una malla '" + name + "'"; return false; }
            Mesh* m = (Mesh*)o;
            std::set<std::string> unique;
            float y0 = 1e30f, y1 = -1e30f;
            for (int i = 0; i < m->vertexSize; i++) {
                unique.insert(std::string((const char*)&m->vertex[i * 3], 12));
                if (m->vertex[i * 3 + 1] < y0) y0 = m->vertex[i * 3 + 1];
                if (m->vertex[i * 3 + 1] > y1) y1 = m->vertex[i * 3 + 1];
            }
            char b[200];
            snprintf(b, sizeof b, "stl revisar: '%s' tiene %d puntos, %d triangulos, Y de %.3f a %.3f (esperaba %d, %d, %.3f a %.3f)",
                     name.c_str(), (int)unique.size(), m->facesSize / 3, y0, y1, points, tris, ymin, ymax);
            printf("      %s\n", b);
            if ((int)unique.size() != points || m->facesSize / 3 != tris || fabsf(y0 - ymin) > 1e-3f || fabsf(y1 - ymax) > 1e-3f) {
                err = b; return false;
            }
            return true;
        }
        if (that == "file") {
            std::string path; int tris = -1; float zmax = 0; ss >> path >> tris >> zmax;
            std::vector<unsigned char> d;
            if (!w3dFileSystem::ReadFileBytes(path, d) || d.size() < 84) { err = "stl archivo: no pude leer " + path; return false; }
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
    // 'coverage': que la FUENTE de la interfaz pueda dibujar cada caracter de cada traduccion (res/languages/*.txt).
    // Usa la fuente de verdad (Font::Soporta: font.png + font-ext.png + las compuestas), no una lista aparte.
    if (cmd == "coverage") {
        extern Font* WhiskFont;
        if (!WhiskFont) { err = "coverage: no hay fuente"; return false; }
        int missing = 0;
        for (int idi = 1; idi < (int)W3dLangCount; idi++) {
            std::vector<unsigned char> b;
            const std::string path = w3dFileSystem::GetResDir() + "/languages/" + W3dLanguageCode((W3dLanguage)idi) + ".txt";
            if (!w3dFileSystem::ReadFileBytes(path, b) || b.empty()) { printf("@cap cobertura %s: sin archivo\n", W3dLanguageCode((W3dLanguage)idi)); continue; }
            b.push_back(0);
            std::set<unsigned> bad;
            size_t i = 0;
            while (b[i]) {
                const uint16_t cp = UTF8_Char((const char*)&b[0], i);
                if (cp == '\n' || cp == '\t' || cp == '\r') continue;
                if (!WhiskFont->Supports(cp)) bad.insert(cp);
            }
            printf("@cap cobertura %s: %d caracter(es) sin dibujo", W3dLanguageCode((W3dLanguage)idi), (int)bad.size());
            for (std::set<unsigned>::iterator it = bad.begin(); it != bad.end(); ++it) printf(" U+%04X", *it);
            printf("\n");
            missing += (int)bad.size();
        }
        if (missing) { err = "coverage: la fuente no tiene todos los caracteres de las traducciones"; return false; }
        return true;
    }
    if (cmd == "capinfo") {
        if (!rootViewport) { err = "capinfo: no hay layout de viewports"; return false; }
        printf("@cap pantalla w=%d h=%d\n", MenuScreenW, MenuScreenH);
        InfoLeaves(rootViewport);
        InfoMenu(OpenMenu, 0);
        // la VENTANA EMERGENTE abierta (un "Guardar como...", el explorador...): su rectangulo (para recortar una captura)
        if (PopUpActive && PopUpActive->popUpWindow)
            printf("@cap popup x=%d y=%d w=%d h=%d name=%s\n", PopUpActive->x, PopUpActive->y, (int)PopUpActive->popUpWindow->width,
                   (int)PopUpActive->popUpWindow->height, PopUpActive->name.c_str());
        // el EXPLORADOR DE ARCHIVOS, si esta abierto: cada archivo / carpeta de la grilla (su rectangulo en la pantalla y
        // su nombre) y los botones de abajo (la accion, como "Abrir", y Cancelar)
        if (FileBrowser* fb = dynamic_cast<FileBrowser*>(PopUpActive)) {
            printf("@cap explorador ruta=%s\n", fb->currentPath.c_str());
            const int cols = fb->cols > 0 ? fb->cols : 1;
            for (size_t k = 0; k < fb->entries.size(); k++) {
                const int ex = fb->fileX + (int)(k % cols) * fb->cellW, ey = fb->fileY + (int)(k / cols) * fb->cellH;
                printf("@cap archivo i=%d x=%d y=%d w=%d h=%d sel=%d name=%s\n", (int)k, ex, ey, fb->cellW, fb->cellH,
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
                if (NumPadKeyCenter(kTec[k], tx, ty)) printf("@cap numpad x=%d y=%d texto=%s\n", tx, ty, kTec[k]);
            }
        }
        // el RELOJ de la Linea de tiempo, si esta abierto: el centro de cada campo (Cuadro actual / Inicio / Fin / FPS)
        {
            extern bool TimePopupField(int j, int& cx, int& cy, std::string& name);
            int cx = 0, cy = 0; std::string name;
            for (int j = 0; j < 4 && TimePopupField(j, cx, cy, name); j++)
                printf("@cap tiempo j=%d x=%d y=%d name=%s\n", j, cx, cy, name.c_str());
        }
        return true;
    }
    // ---- CONTROLES (ver arriba): las pruebas de lo basico ----
    // 'topology <malla> <puntos> <aristas> <caras> [sueltas]': posiciones distintas, aristas, caras y lineas sueltas.
    // Asi se ve que una operacion no dejo restos (Separar) ni le falto una cara (Extruir un plano)
    if (cmd == "topology") {
        std::string name; int p = -1, a = -1, c = -1, su = -1; ss >> name >> p >> a >> c; if (!(ss >> su)) su = -1;
        Mesh* m = MeshCall(name);
        if (!m) { err = "topology: no hay una malla '" + name + "'"; return false; }
        std::set<std::string> u;
        for (int i = 0; i < m->vertexSize; i++) u.insert(std::string((const char*)&m->vertex[i * 3], 12));
        const int np = (int)u.size(), na = (int)(m->edges.size() / 2), nc = (int)m->faces3d.size(), ns = (int)(m->looseEdges.size() / 2);
        char b[240];
        snprintf(b, sizeof b, "topology '%s': %d puntos, %d aristas, %d caras, %d sueltas (esperaba %d, %d, %d, %d)", name.c_str(), np, na, nc, ns, p, a, c, su);
        printf("      %s\n", b);
        if (np != p || na != a || nc != c || (su >= 0 && ns != su)) { err = b; return false; }
        return true;
    }
    // 'volume <malla> <min> <max>': el volumen con signo (en local) de una malla cerrada. Positivo = todas las caras
    // miran para AFUERA; una cara dada vuelta o que falta lo cambia
    if (cmd == "volume") {
        std::string name; float mn = 0, mx = 0; ss >> name >> mn >> mx;
        Mesh* m = MeshCall(name);
        if (!m) { err = "volume: no hay una malla '" + name + "'"; return false; }
        double v = 0;
        for (size_t f = 0; f < m->faces3d.size(); f++) {
            const std::vector<int>& ix = m->faces3d[f].idx;
            for (size_t k = 1; k + 1 < ix.size(); k++) {
                const GLfloat* a = &m->vertex[ix[0] * 3]; const GLfloat* b = &m->vertex[ix[k] * 3]; const GLfloat* c = &m->vertex[ix[k + 1] * 3];
                v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0;
            }
        }
        printf("      [volumen] '%s' = %.4f (esperaba %.4f a %.4f)\n", name.c_str(), v, mn, mx);
        if (v < mn || v > mx) { char b[160]; snprintf(b, sizeof b, "volume '%s': %.4f, fuera de %.4f a %.4f", name.c_str(), v, mn, mx); err = b; return false; }
        return true;
    }
    // 'worldbox <malla> <xmin> <ymin> <zmin> <xmax> <ymax> <zmax> <tol>': donde esta la malla EN EL MUNDO (interno, Y
    // arriba). Lo que no se tiene que mover (Origen al Cursor 3D mueve el origen, no la geometria)
    if (cmd == "worldbox") {
        std::string name; float e[6] = { 0, 0, 0, 0, 0, 0 }, tol = 0.001f; ss >> name >> e[0] >> e[1] >> e[2] >> e[3] >> e[4] >> e[5] >> tol;
        Mesh* m = MeshCall(name);
        if (!m || m->vertexSize <= 0) { err = "worldbox: no hay una malla '" + name + "'"; return false; }
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
        snprintf(b, sizeof b, "worldbox '%s': %.3f %.3f %.3f a %.3f %.3f %.3f (esperaba %.3f %.3f %.3f a %.3f %.3f %.3f)", name.c_str(),
                 mn[0], mn[1], mn[2], mx[0], mx[1], mx[2], e[0], e[1], e[2], e[3], e[4], e[5]);
        printf("      %s\n", b);
        for (int k = 0; k < 3; k++) if (fabsf(mn[k] - e[k]) > tol || fabsf(mx[k] - e[k + 3]) > tol) { err = b; return false; }
        return true;
    }
    // 'checkactive <nombre>': el objeto activo es ese
    // 'propsforget': ningun panel de Propiedades es el activo (como recien abierto el editor, antes de tocar uno)
    if (cmd == "propsforget") { PropsActive = NULL; return true; }
    // 'renderpasses <zbuffer> <normal> <alfa>': los tildes de los pases extra de la tarjeta Render (0/1)
    if (cmd == "renderpasses") {
        int z = 0, n = 0, a = 0; ss >> z >> n >> a;
        g_render.zbuffer = (z != 0); g_render.normal = (n != 0); g_render.alpha = (a != 0);
        extern void RenderRowsFormat(Properties* P); RenderRowsFormat(PropsActive);
        return true;
    }
    // 'renderformat <png|jpg|mkv|mp4> [calidad]': el formato de la tarjeta Render (como el menu Formato)
    if (cmd == "renderformat") {
        std::string f; float q = -1.0f; ss >> f >> q;
        const int id = (f == "png") ? 0 : (f == "jpg") ? 1 : (f == "mkv") ? 2 : (f == "mp4") ? 3 : -1;
        if (id < 0) { err = "renderformat: png|jpg|mkv|mp4"; return false; }
        g_render.format = id;
        if (q > 0.0f) g_render.quality = q;
        extern void RenderRowsFormat(Properties* P); RenderRowsFormat(PropsActive);
        return true;
    }
    // 'renderaudio <wav|flac|mp3|aac|opus> [kbps] [sonido 0|1]': el formato de audio de la tarjeta Render (el del boton
    // "Renderizar audio" y el del sonido de los videos) y el tilde "Con sonido"
    if (cmd == "renderaudio") {
        std::string f; float k = -1.0f; int so = -1; ss >> f >> k >> so;
        const int id = (f == "wav") ? 0 : (f == "flac") ? 1 : (f == "mp3") ? 2 : (f == "aac") ? 3 : (f == "opus") ? 4 : -1;
        if (id < 0) { err = "renderaudio: wav|flac|mp3|aac|opus"; return false; }
        g_render.audioFormat = id;
        if (k > 0.0f) g_render.audioKbps = k;
        if (so >= 0) g_render.sound = (so != 0);
        extern void RenderRowsFormat(Properties* P); RenderRowsFormat(PropsActive);
        return true;
    }
    // 'renderaudiobutton <carpeta> <nombre>': el boton "Renderizar audio" (con la ruta y el nombre del render)
    if (cmd == "renderaudiobutton") {
        std::string d, n; ss >> d >> n;
        if (!d.empty()) g_render.folder = d;
        if (!n.empty()) g_render.name = n;
        extern void W3dHarnessRenderAudio(); W3dHarnessRenderAudio();
        return true;
    }
    // 'wavinfo <ruta> <segundos> [tol] [rms minimo]': un WAV de 16 bits estereo: cuanto dura y que no sea silencio
    if (cmd == "wavinfo") {
        std::string path; float seg = 0, tol = 0.02f, rmsMin = 50.0f; ss >> path >> seg >> tol >> rmsMin;
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) { err = "wavinfo: no se puede abrir " + path; return false; }
        unsigned char cab[44]; const bool read = fread(cab, 1, 44, f) == 44;
        if (!read || memcmp(cab, "RIFF", 4) || memcmp(cab + 8, "WAVE", 4)) { fclose(f); err = "wavinfo: no es un WAV"; return false; }
        const int rate = cab[24] | (cab[25] << 8) | (cab[26] << 16) | (cab[27] << 24);
        const unsigned data = cab[40] | (cab[41] << 8) | (cab[42] << 16) | ((unsigned)cab[43] << 24);
        std::vector<short> m(data / 2);
        const size_t n = m.empty() ? 0 : fread(&m[0], 2, m.size(), f);
        fclose(f);
        double sum = 0; for (size_t i = 0; i < n; i++) sum += (double)m[i] * m[i];
        const double rms = n ? sqrt(sum / n) : 0.0, dur = rate > 0 ? (double)data / 4.0 / rate : 0.0;
        printf("      [wavinfo] %s: %d Hz, %.3f s, rms %.1f\n", path.c_str(), rate, dur, rms);
        if (fabs(dur - seg) > tol) { err = "wavinfo: no dura lo esperado"; return false; }
        if (rms < rmsMin) { err = "wavinfo: es silencio"; return false; }
        return true;
    }
    // 'mediatracks <ruta> <video 0|1> <audio 0|1> [segundos tol]': las pistas de un archivo (ffprobe). Sin ffmpeg en la
    // maquina se SALTEA con "SIN FFMPEG:"
    if (cmd == "mediatracks") {
        std::string path; int v = 0, a = 0; float seg = -1, tol = 0.1f; ss >> path >> v >> a >> seg >> tol;
        if (system("ffprobe -version >/dev/null 2>&1") != 0) { printf("      SIN FFMPEG: se saltea el control de %s\n", path.c_str()); return true; }
        FILE* p = popen(("ffprobe -v error -show_entries stream=codec_type:format=duration -of csv=p=0 \"" + path + "\" 2>/dev/null").c_str(), "r");
        if (!p) { err = "mediatracks: no se pudo correr ffprobe"; return false; }
        char line[256]; int hv = 0, ha = 0; double dur = -1;
        while (fgets(line, sizeof(line), p)) {
            if (strstr(line, "video")) hv++; else if (strstr(line, "audio")) ha++; else dur = atof(line);
        }
        pclose(p);
        printf("      [mediapistas] %s: %d de video, %d de audio, %.3f s\n", path.c_str(), hv, ha, dur);
        if ((hv > 0) != (v != 0) || (ha > 0) != (a != 0)) { err = "mediatracks: las pistas no son las esperadas"; return false; }
        if (seg >= 0 && fabs(dur - seg) > tol) { err = "mediatracks: no dura lo esperado"; return false; }
        return true;
    }
    // 'renderfinal <composicion final> <con alfa> <postprocesado>': los tildes de la tarjeta Render (0/1)
    if (cmd == "renderfinal") {
        int f = 1, a = 0, po = 1; ss >> f >> a >> po;
        g_render.final = (f != 0); g_render.finalAlpha = (a != 0); g_render.post = (po != 0);
        extern void RenderRowsFormat(Properties* P); RenderRowsFormat(PropsActive);
        return true;
    }
    // 'videoframes <ruta> <cuadros> <ancho> <alto>': el video que escribio ffmpeg (ffprobe cuenta sus cuadros). Sin ffmpeg
    // en la maquina se SALTEA con "SIN FFMPEG:" (los videos del render son con el ffmpeg instalado)
    if (cmd == "videoframes") {
        std::string path; int n = 0, w = 0, h = 0; ss >> path >> n >> w >> h;
        FILE* p = popen(("ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames,width,height "
                         "-of csv=p=0 \"" + path + "\" 2>/dev/null").c_str(), "r");
        char line[256]  = { 0 };
        const bool read = p && fgets(line, sizeof(line), p);
        const int end = p ? pclose(p) : -1;
        if (!read && end != 0 && system("ffprobe -version >/dev/null 2>&1") != 0) {
            printf("      SIN FFMPEG: se saltea el control de %s\n", path.c_str());
            return true;
        }
        int vw = 0, vh = 0, vn = 0;
        if (!read || sscanf(line, "%d,%d,%d", &vw, &vh, &vn) != 3) { err = "videoframes: no se puede leer el video " + path; return false; }
        printf("      [videocuadros] %s: %dx%d, %d cuadros\n", path.c_str(), vw, vh, vn);
        if (vn != n || vw != w || vh != h) { err = "videoframes: el video no es el esperado"; return false; }
        return true;
    }
    // 'renders <campo> <valor>': ASSERTA una opcion de la tarjeta Render (g_render): formato calidad ancho alto final
    // finalAlpha post zbuffer normal alfa sonido audio kbps nombre
    if (cmd == "renders") {
        std::string c, v; ss >> c; v = Rest(ss);
        const W3dRenderOpt& r = g_render;
        char b[64]; std::string has;
        if (c == "format") { snprintf(b, sizeof b, "%d", r.format); has = b; }
        else if (c == "quality") { snprintf(b, sizeof b, "%g", r.quality); has = b; }
        else if (c == "width") { snprintf(b, sizeof b, "%g", r.w); has = b; }
        else if (c == "height") { snprintf(b, sizeof b, "%g", r.h); has = b; }
        else if (c == "kbps") { snprintf(b, sizeof b, "%g", r.audioKbps); has = b; }
        else if (c == "audio") { snprintf(b, sizeof b, "%d", r.audioFormat); has = b; }
        else if (c == "final") has = r.final ? "1" : "0";
        else if (c == "finalAlpha") has = r.finalAlpha ? "1" : "0";
        else if (c == "post") has = r.post ? "1" : "0";
        else if (c == "zbuffer") has = r.zbuffer ? "1" : "0";
        else if (c == "normal") has = r.normal ? "1" : "0";
        else if (c == "alpha") has = r.alpha ? "1" : "0";
        else if (c == "sound") has = r.sound ? "1" : "0";
        else if (c == "name") has = r.name;
        else { err = "renders: campo desconocido " + c; return false; }
        printf("      [renderes] %s = %s (esperado %s)\n", c.c_str(), has.c_str(), v.c_str());
        if (has != v) { err = "renders: " + c + " es " + has + ", se esperaba " + v; return false; }
        return true;
    }
    // 'rendersize <ancho> <alto>': el tamano del render a archivo (las filas Ancho / Alto de la tarjeta Render)
    // ---- renderaa <1..4> : el ANTIALIASING del render rasterizado (supermuestreo; 1 = apagado) ----
    if (cmd == "renderaa") { int v = 1; ss >> v; g_render.antialias = v < 1 ? 1 : (v > 4 ? 4 : v); return true; }
    if (cmd == "rendersize") { float w = 0, h = 0; ss >> w >> h; if (w < 1 || h < 1) { err = "rendersize: <ancho> <alto>"; return false; } g_render.w = w; g_render.h = h; return true; }
    // 'renderstripes <0|1>': 0 = el render a archivo con la imagen entera en memoria (para comparar con las franjas)
    if (cmd == "renderstripes") { int v = 1; ss >> v; extern bool g_renderNoStripes; g_renderNoStripes = (v == 0); return true; }
    // 'pngdiff <a> <b> <media maxima> [maximo]': dos imagenes del mismo tamano: la diferencia media por canal (0..255) y la
    // mayor. Rasterizadas por franjas y enteras tienen que ser iguales; trazadas, iguales salvo el ruido
    // 'buffershot <archivo.png>': lo que hay en la pantalla AHORA, sin volver a dibujar (uishot redibuja todo): el
    // cuadro que dejo el ultimo tick, por ejemplo uno de los paneles guardados mientras se navega
    if (cmd == "buffershot") {
        std::string path; ss >> path;
        const int w = MenuScreenW, h = W3dScreenHeight;
        if (path.empty() || w < 1 || h < 1) { err = "buffershot: falta la ruta o la pantalla no tiene tamano"; return false; }
        w3dEngine::Finish();
        std::vector<unsigned char> px((size_t)w * h * 4, 0);
        w3dEngine::ReadPixelsRGBA(0, 0, w, h, &px[0]);
        if (!w3dEngine::SavePNG(path.c_str(), &px[0], w, h, true)) { err = "buffershot: no se pudo escribir " + path; return false; }
        printf("      [buffershot] %dx%d -> %s\n", w, h, path.c_str());
        return true;
    }
    // 'viewcache <n>': cuantos cuadros salieron de la imagen guardada de los paneles desde el ultimo 'viewcache' (tiene
    // que ser n)
    if (cmd == "viewcache") {
        int want = -1; ss >> want;
        extern int g_viewCacheFrames;
        const int got = g_viewCacheFrames; g_viewCacheFrames = 0;
        printf("      [viewcache] %d cuadro(s) con los paneles guardados\n", got);
        if (want >= 0 && got != want) { err = "viewcache: se esperaban " + std::to_string(want); return false; }
        return true;
    }
    if (cmd == "pngdiff") {
        std::string a, b; float meanMax = 0, maxMax = 255; ss >> a >> b >> meanMax >> maxMax;
        int wa = 0, ha = 0, wb = 0, hb = 0, c = 0;
        unsigned char* pa = stbi_load(a.c_str(), &wa, &ha, &c, 4);
        unsigned char* pb = stbi_load(b.c_str(), &wb, &hb, &c, 4);
        if (!pa || !pb || wa != wb || ha != hb) {
            if (pa) stbi_image_free(pa); if (pb) stbi_image_free(pb);
            err = "pngdiff: no se pueden leer o no son del mismo tamano"; return false;
        }
        double sum = 0; int mx = 0;
        const size_t n = (size_t)wa * ha;
        for (size_t i = 0; i < n; i++) for (int k = 0; k < 3; k++) { const int d = abs((int)pa[i * 4 + k] - (int)pb[i * 4 + k]); sum += d; if (d > mx) mx = d; }
        stbi_image_free(pa); stbi_image_free(pb);
        const double mean = sum / (n * 3.0);
        printf("      [pngdif] %dx%d: media %.3f, mayor %d\n", wa, ha, mean, mx);
        if (mean > meanMax || mx > maxMax) { err = "pngdiff: las imagenes son distintas"; return false; }
        return true;
    }
    // 'pngpx <ruta> <x> <y> <r> <g> <b> [a|-] [tol]': el color de un pixel de un PNG guardado (x, y desde arriba a la
    // izquierda), con tolerancia por canal (2 si no se dice). '-' = no mirar el alfa
    if (cmd == "pngpx") {
        std::string path, sa; int x = 0, y = 0, r = 0, g = 0, b = 0, tol = 2; ss >> path >> x >> y >> r >> g >> b >> sa >> tol;
        int w = 0, h = 0, c = 0;
        unsigned char* px = stbi_load(path.c_str(), &w, &h, &c, 4);
        if (!px) { err = "pngpx: no se puede leer " + path; return false; }
        if (x < 0 || y < 0 || x >= w || y >= h) { stbi_image_free(px); err = "pngpx: el pixel esta afuera de la imagen"; return false; }
        const unsigned char* q = px + ((size_t)y * w + x) * 4;
        const int has[4] = { q[0], q[1], q[2], q[3] };
        stbi_image_free(px);
        printf("      [pngpx] %s (%d,%d) = %d %d %d %d (%dx%d, %d canales)\n", path.c_str(), x, y, has[0], has[1], has[2], has[3], w, h, c);
        const int esp[3] = { r, g, b };
        for (int k = 0; k < 3; k++) if (abs(has[k] - esp[k]) > tol) { err = "pngpx: el color no es el esperado"; return false; }
        if (!sa.empty() && sa != "-" && abs(has[3] - atoi(sa.c_str())) > tol) { err = "pngpx: el alfa no es el esperado"; return false; }
        return true;
    }
    if (cmd == "checkactive") {
        const std::string name = Rest(ss);
        const std::string has  = ActiveObj ? ActiveObj->name : std::string("(ninguno)");
        printf("      [activo] %s (esperaba %s)\n", has.c_str(), name.c_str());
        if (has != name) { err = "checkactive: es '" + has + "', se esperaba '" + name + "'"; return false; }
        return true;
    }
    // 'mouseview3d': el mouse al centro de la Vista 3D (un movimiento de verdad: el foco va con el mouse)
    if (cmd == "mouseview3d") {
        ViewportBase* v = TypeLeaf(rootViewport, 1);
        if (!v) { err = "mouseview3d: no hay una Vista 3D"; return false; }
        const int x = v->x + v->width / 2, y = v->y + v->height / 2;
        Moved(0, gMouseButtons, x, y, gMouseX, gMouseY); gMouseX = x; gMouseY = y;
        return true;
    }
    // 'menuitem <texto en ingles>': el menu abierto tiene esa opcion (traducida al idioma del editor)
    if (cmd == "menuitem") {
        const std::string t = T(Rest(ss).c_str());
        extern PopupMenu* OpenMenu;
        bool has = false;
        if (OpenMenu && OpenMenu->open)
            for (size_t i = 0; i < OpenMenu->items.size(); i++) if (OpenMenu->items[i]->text == t) has = true;
        printf("      [menuitem] '%s': %s\n", t.c_str(), has ? "is" : "NO esta");
        if (!has) { err = "menuitem: el menu abierto no tiene '" + t + "'"; return false; }
        return true;
    }
    // 'propclick <fila en ingles>': un click de verdad en el valor de esa fila de Propiedades (una casilla, un boton)
    if (cmd == "propclick") {
        const std::string t = T(Rest(ss).c_str());
        int x = 0, y = 0;
        // si la fila esta pero fuera de la vista, se desplaza Propiedades con la rueda hasta que se vea (como la persona)
        Properties* P = (Properties*)TypeLeaf(rootViewport, 3);
        for (int turn = 0; turn < 40; turn++) {
            PropertieBase* pb = NULL;
            if (RowProperties(t, x, y, &pb)) break;
            if (!pb || !P) break;                              // no existe: no hay nada que desplazar
            P->event_mouse_wheel(y > P->y + P->height / 2 ? -1.0f : 1.0f, P->x + P->width / 2, P->y + P->height / 2);
            Frame();
        }
        if (!RowProperties(t, x, y, NULL)) { err = "propclick: no se ve la fila '" + t + "' en Propiedades" + VisibleRows(); return false; }
        ClickIn(x, y, 1);
        return true;
    }
    // 'proprightclick <fila en ingles>': un click DERECHO de verdad en esa fila de Propiedades (su menu contextual:
    // restablecer, copiar, pegar, los keyframes). 'propfinger <fila>': el DEDO apoyado ahi (y no se levanta: despues
    // 'wait' + 'tick' es la pulsacion larga, y 'finger up' lo suelta)
    if (cmd == "proprightclick" || cmd == "propfinger" || cmd == "propfingerdrag") {
        int dxArr = 0;
        if (cmd == "propfingerdrag") ss >> dxArr;   // 'propfingerdrag <dx> <fila>': apoyar, correr dx y soltar
        const std::string t = T(Rest(ss).c_str());
        int x = 0, y = 0;
        // (como propclic: si la fila esta fuera de la vista, se desplaza Propiedades con la rueda hasta que se vea BIEN:
        // con el centro en el ultimo renglon, el toque caia en el borde del panel)
        Properties* P = (Properties*)TypeLeaf(rootViewport, 3);
        for (int turn = 0; turn < 40; turn++) {
            PropertieBase* pb = NULL;
            if (RowProperties(t, x, y, &pb) && P && y < P->y + P->height - RowHeightGS) break;
            if (!pb || !P) break;
            P->event_mouse_wheel(y > P->y + P->height / 2 ? -1.0f : 1.0f, P->x + P->width / 2, P->y + P->height / 2);
            Frame();
        }
        PropertieBase* row = NULL;
        if (!RowProperties(t, x, y, &row)) { err = cmd + ": no se ve la fila '" + t + "' en Propiedades" + VisibleRows(); return false; }
        // una fila BOTON con su numero en el medio (los mapas del material): el dedo, sobre el numero
        if (row && row->GetType() == PropertyType::Button && P) {
            PropButton* pb = (PropButton*)row;
            if (pb->colExtras > pb->colPrefix) x = P->x + P->PosX + borderGS * 2 + (pb->colPrefix + pb->colExtras) / 2;
        }
        if (cmd == "proprightclick") {
            if (x != gMouseX || y != gMouseY) Moved(0, gMouseButtons, x, y, gMouseX, gMouseY);
            gMouseX = x; gMouseY = y;
            SendButton(0, SDL_BUTTON_RIGHT, true, x, y);
            SendButton(0, SDL_BUTTON_RIGHT, false, x, y);
        } else {
            Moved(SDL_TOUCH_MOUSEID, 0, x, y, x, y); SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x, y);
            SendFinger(SDL_FINGERDOWN, 1, x, y, x, y);
            gFingers[1] = std::make_pair(x, y);
            if (cmd == "propfingerdrag") {   // de a 4 pasos, como un dedo de verdad, y se levanta al final
                int px = x;
                for (int k = 1; k <= 4; k++) {
                    const int nx = x + dxArr * k / 4;
                    Moved(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, nx, y, px, y); SendFinger(SDL_FINGERMOTION, 1, nx, y, px, y);
                    px = nx;
                }
                SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, px, y); SendFinger(SDL_FINGERUP, 1, px, y, px, y);
                gFingers.erase(1);
            }
        }
        printf("      [%s] '%s' en (%d, %d)\n", cmd.c_str(), t.c_str(), x, y);
        Frame();
        return true;
    }
    // 'propvalue <valor> <fila en ingles>': el valor que muestra esa fila (un numero, o 0/1 de una casilla)
    if (cmd == "propvalue") {
        float expected = 0; ss >> expected;
        const std::string t = T(Rest(ss).c_str());
        int x = 0, y = 0; PropertieBase* pb = NULL;
        RowProperties(t, x, y, &pb);
        float has = -12345.0f;
        if (pb && pb->GetType() == PropertyType::Float && ((PropFloat*)pb)->value) has = *((PropFloat*)pb)->value;
        else if (pb && pb->GetType() == PropertyType::Bool && ((PropBool*)pb)->value) has = *((PropBool*)pb)->value ? 1.0f : 0.0f;
        printf("      [propvalor] '%s' = %g (esperaba %g)\n", t.c_str(), has, expected);
        if (fabsf(has - expected) > 1e-4f) { err = "propvalue: '" + t + "' no tiene el valor esperado"; return false; }
        return true;
    }
    // 'layout <sub>': LOS LAYOUTS (la esquina de un viewport -> Layout; getConfig/W3dLayouts.h) y Rotar:
    //    layout structure <forma>    el arbol como texto corto: F(a,b) = fila (lado a lado), C(a,b) = columna (apilados);
    //                                hojas: 3D, TL (timeline), OUT, BIB, PROP, UV, 2D, CON, IDE
    //    layout save <nombre>        guarda el actual (lo mismo que la ventanita del nombre)
    //    layout set <nombre|default>     lo pide y lo pone (como el menu: al empezar el cuadro)
    //    layout delete <nombre> | layout count <n>
    //    layout rotate <tipo>        Rotar en el primer viewport de ese tipo (1 = 3D, 5 = timeline...)
    //    layout expand|close <tipo>  Expandir / Cerrar viewport en el primer viewport de ese tipo
    //    layout phone 0|1            como si fuera un telefono (un layout por orientacion) o no
    //    layout screen <w> <h>       la ventana cambia de tamano (girar el telefono)
    if (cmd == "layout") {
        std::string sub; ss >> sub;
        if (sub == "structure") {
            std::string esp; ss >> esp;
            struct F { static std::string Of(ViewportBase* v) {
                if (!v) return "?";
                if (!v->isLeaf()) {
                    ViewportBase* a = (v->ContainerKind() == 1) ? ((ViewportRow*)v)->childA : ((ViewportColumn*)v)->childA;
                    ViewportBase* b = (v->ContainerKind() == 1) ? ((ViewportRow*)v)->childB : ((ViewportColumn*)v)->childB;
                    return std::string(v->ContainerKind() == 1 ? "F(" : "C(") + Of(a) + "," + Of(b) + ")";
                }
                switch (v->ViewportKind()) { case 1: return "3D"; case 2: return "OUT"; case 3: return "PROP"; case 4: return "UV";
                    case 5: return "TL"; case 6: return "2D"; case 7: return "CON"; case 8: return "IDE"; case 9: return "BIB"; }
                return "?"; } };
            extern ViewportBase* LayoutCompleteRoot();
            const std::string has = F::Of(LayoutCompleteRoot());
            printf("      [layout] estructura %s (esperaba %s)\n", has.c_str(), esp.c_str());
            if (!esp.empty() && has != esp) { err = "layout estructura: es " + has; return false; }
            return true;
        }
        if (sub == "save" || sub == "set" || sub == "delete") {
            std::string n = Rest(ss);
            extern void LayoutRequest(const std::string&); extern void ProcessPendingProject();
            extern std::string W3dLayoutTextOf(ViewportBase*);
            extern bool W3dLayoutSave(const std::string&, const std::string&); extern bool W3dLayoutDelete(const std::string&);
            if (sub == "save") { if (!W3dLayoutSave(n, W3dLayoutTextOf(NULL))) { err = "layout guardar: no se guardo"; return false; } }
            else if (sub == "delete") { if (!W3dLayoutDelete(n)) { err = "layout borrar: no existe '" + n + "'"; return false; } }
            else { LayoutRequest(n == "default" ? std::string() : n); ProcessPendingProject(); Frame(); }
            return true;
        }
        if (sub == "count") {
            int esp = -1; ss >> esp;
            extern std::vector<std::string> W3dLayoutsNames();
            std::vector<std::string> ns = W3dLayoutsNames();
            std::string l; for (size_t i = 0; i < ns.size(); i++) l += (i ? ", " : "") + ns[i];
            printf("      [layout] %d guardados: %s\n", (int)ns.size(), l.c_str());
            if (esp >= 0 && (int)ns.size() != esp) { err = "layout cuantos: no son los esperados"; return false; }
            return true;
        }
        if (sub == "rotate") {
            int kind = 1; ss >> kind;
            ViewportBase* v = TypeLeaf(rootViewport, kind);
            if (!v) { err = "layout rotate: no hay un viewport de ese tipo"; return false; }
            extern void LayoutRotateViewport(ViewportBase*); LayoutRotateViewport(v); Frame();
            return true;
        }
        // layout expand <tipo> | layout close <tipo>: Expandir / Cerrar viewport del menu de la esquina en el primer
        // viewport de ese tipo. Despues, el activo tiene que ser un viewport que sigue en el arbol
        if (sub == "expand" || sub == "close") {
            int kind = 1; ss >> kind;
            ViewportBase* v = TypeLeaf(rootViewport, kind);
            if (!v) { err = "layout " + sub + ": no hay un viewport de ese tipo"; return false; }
            extern void LayoutChangeViewportType(ViewportBase* v, int aId);
            LayoutChangeViewportType(v, sub == "expand" ? 20 : 27); Frame();
            struct E { static bool In(ViewportBase* n, ViewportBase* x) {
                if (!n) return false;
                if (n == x) return true;
                if (n->isLeaf()) return false;
                return n->ContainerKind() == 1 ? (In(((ViewportRow*)n)->childA, x) || In(((ViewportRow*)n)->childB, x))
                                               : (In(((ViewportColumn*)n)->childA, x) || In(((ViewportColumn*)n)->childB, x)); } };
            if (!viewPortActive || !viewPortActive->isLeaf() || !E::In(rootViewport, viewPortActive)) {
                err = "layout " + sub + ": el viewport activo no quedo en el arbol"; return false;
            }
            return true;
        }
        if (sub == "phone") {
            int v = 1; ss >> v;
            extern bool g_w3dPhone; g_w3dPhone = (v != 0);
            extern void LayoutOrientationForget(); LayoutOrientationForget();
            extern void LayoutScreenSize(int, int); LayoutScreenSize(MenuScreenW, MenuScreenH);   // (la de ahora)
            return true;
        }
        if (sub == "screen") {
            int w = 0, h = 0; ss >> w >> h;
            if (w <= 0 || h <= 0) { err = "layout pantalla <w> <h>"; return false; }
            extern int W3dScreenHeight;
            if (window) SDL_SetWindowSize(window, w, h);
            MenuScreenW = w; MenuScreenH = h; W3dScreenHeight = h;
            rootViewport->Resize(w, h);
            extern void LayoutScreenSize(int, int); LayoutScreenSize(w, h);
            Frame();
            return true;
        }
        if (sub == "menu") {   // 'layout menu save|delete': esa opcion del submenu Layout (la ventanita / la lista)
            std::string q; ss >> q;
            extern void LayoutMenuLayoutsAction(int); LayoutMenuLayoutsAction(q == "delete" ? 901 : 900); Frame();
            return true;
        }
        err = "layout: uso: layout structure|save|set|delete|count|rotate|expand|close|phone|screen|menu"; return false;
    }
    // EL TECLADO EN PANTALLA (Ajustes > "Use system keyboard"):
    //  'keyboard sistema|whisk3d'               la opcion (sin guardar el config)
    //  'keyboardopen qwerty|numpad|ninguno'  el teclado de Whisk3D que quedo abierto (el popup activo)
    //  'keyboardtouch <tecla>...'               toca esas teclas del teclado abierto ("0" "." "2" "OK"...; QWERTY: solo OK)
    if (cmd == "keyboard") {
        std::string q; ss >> q;
        if (q != "system" && q != "whisk3d") { err = "keyboard: uso: keyboard sistema|whisk3d"; return false; }
        cfg.keyboardSystem = (q == "system");
        return true;
    }
    if (cmd == "keyboardopen") {
        std::string q; ss >> q;
        const std::string has = !PopUpActive ? std::string("none") : PopUpActive->name == "Qwerty" ? std::string("qwerty")
                              : PopUpActive->name == "NumPad" ? std::string("numpad") : std::string("otro (") + PopUpActive->name + ")";
        printf("      [tecladoabierto] %s\n", has.c_str());
        const bool isKeyboard = PopUpActive && PopUpActive->IsKeyboard();
        if (q == "none" ? isKeyboard : has != q) { err = "keyboardopen: esta abierto '" + has + "', se esperaba '" + q + "'"; return false; }
        return true;
    }
    if (cmd == "keyboardtouch") {
        std::string t;
        while (ss >> t) {
            if (!PopUpActive || !PopUpActive->IsKeyboard()) { err = "keyboardtouch: no hay un teclado de Whisk3D abierto"; return false; }
            if (PopUpActive->name == "Qwerty") {
                if (t != "OK") { err = "keyboardtouch: en el QWERTY solo se toca OK"; return false; }
                PopUpActive->Key(LayoutKey::Accept);   // (Enter = Aceptar)
            } else {
                int cx = 0, cy = 0;
                if (!NumPadKeyCenter(t.c_str(), cx, cy)) { err = "keyboardtouch: el teclado no tiene la tecla '" + t + "'"; return false; }
                PopUpActive->Click(cx, cy);
            }
            Frame();
        }
        return true;
    }
    // 'diamond camactiva|camchk [toggle] [<estado>]': el rombo de la CAMARA ACTIVA como lo ve el panel (W3dKeyframeState):
    // camactiva = el de la tarjeta Camara activa de la animacion; camchk = el de la casilla "Camara activa" de la camara
    // activa en Propiedades. 'toggle' es el click en el rombo. Estado: 0 nada, 1 verde oscuro, 2 verde, 3 cambio sin guardar
    if (cmd == "diamond") {
        std::string that, q2; ss >> that;
        const int comp = (that == "camchk") ? AnimY : AnimX;
        int esp = -1;
        if (ss >> q2) { if (q2 == "toggle") { if (W3dKeyframeToggle) W3dKeyframeToggle(AnimCamActive, comp); ss >> esp; } else esp = atoi(q2.c_str()); }
        const int has = W3dKeyframeState ? W3dKeyframeState(AnimCamActive, comp) : -1;
        printf("      [rombo] %s = %d (esperaba %d)\n", that.c_str(), has, esp);
        if (esp >= 0 && has != esp) { err = "diamond: el estado no es el esperado"; return false; }
        return true;
    }
    // 'channelstate <prop> <comp> <estado>': el keyframe del canal del objeto activo en el frame actual (como el rombo):
    // 0 sin animacion, 1 animado sin key aca, 2 key aca. prop: 0 posicion, 1 rotacion, 2 escala, 3 visible...
    if (cmd == "channelstate") {
        int prop = 0, comp = 0, esp = 0; ss >> prop >> comp >> esp;
        extern int AnimChannelState(Object*, int, int, int); extern int CurrentFrame;
        const int has = AnimChannelState(ActiveObj, prop, comp, CurrentFrame);
        printf("      [canalestado] %d %d en el frame %d = %d (esperaba %d)\n", prop, comp, CurrentFrame, has, esp);
        if (has != esp) { err = "channelstate: el estado no es el esperado"; return false; }
        return true;
    }
    // 'propbutton <fila en ingles> | <texto>': lo que dice el boton de esa fila (el eje del Tornillo: 'Eje | Z')
    if (cmd == "propbutton") {
        const std::string r = Rest(ss); const size_t bar = r.find('|');
        if (bar == std::string::npos) { err = "propbutton: uso: propbutton <fila> | <texto>"; return false; }
        std::string row = r.substr(0, bar), txt = r.substr(bar + 1);
        while (!row.empty() && row[row.size() - 1] == ' ') row.erase(row.size() - 1);
        while (!txt.empty() && txt[0] == ' ') txt.erase(0, 1);
        int x = 0, y = 0; PropertieBase* pb = NULL;
        RowProperties(T(row.c_str()), x, y, &pb);
        if (!pb || pb->GetType() != PropertyType::Button) { err = "propbutton: no hay un boton '" + row + "'"; return false; }
        const std::string has = ((PropButton*)pb)->button->text;
        printf("      [propboton] %s = '%s' (esperaba '%s')\n", row.c_str(), has.c_str(), txt.c_str());
        if (has != txt) { err = "propbutton: '" + row + "' dice '" + has + "', se esperaba '" + txt + "'"; return false; }
        return true;
    }
    // 'modaxis <x|y|z> <0|1>': el eje INTERNO (Y arriba) del modificador activo de la malla activa (Mirror)
    if (cmd == "modaxis") {
        std::string e; int v = -1; ss >> e >> v;
        Mesh* m = (ActiveObj && ActiveObj->getType() == ObjectType::mesh) ? (Mesh*)ActiveObj : NULL;
        if (!m || m->modifiers.empty()) { err = "modaxis: la malla activa no tiene modificadores"; return false; }
        int ia = m->activeModifier; if (ia < 0 || ia >= (int)m->modifiers.size()) ia = (int)m->modifiers.size() - 1;
        Modifier* mod = m->modifiers[ia];
        const bool has = (e == "x") ? mod->axisX : (e == "y") ? mod->axisY : mod->axisZ;
        printf("      [modeje] %s interno = %d (esperaba %d)\n", e.c_str(), has ? 1 : 0, v);
        if ((has ? 1 : 0) != v) { err = "modaxis: el eje interno " + e + " no quedo como se esperaba"; return false; }
        return true;
    }
    // 'renamerow <objeto> <nombre nuevo>': como una persona: doble click en la fila del Outliner, escribe el
    // nombre (Shift en las mayusculas) y Enter
    if (cmd == "renamerow") {
        std::string name, newValue; ss >> name >> newValue;
        Outliner* o = (Outliner*)TypeLeaf(rootViewport, 2);
        int x = 0, y = 0;
        if (!o || !RowOutlinerOf(o, SceneCollection, name, x, y)) { err = "renamerow: no se ve la fila '" + name + "'"; return false; }
        ClickIn(x, y, 2);
        Write(newValue);
        KeyEv(SDLK_RETURN, true, 0); KeyEv(SDLK_RETURN, false, 0);
        return true;
    }
    // 'viewlocal <objeto> <0|1>': ese objeto se ve (1) o no (0) en la vista local de la Vista 3D activa. Lo que
    // nace mientras dura (Separar con P) se tiene que ver: si no, desaparece y parece un error
    if (cmd == "viewlocal") {
        std::string name; int v = -1; ss >> name >> v;
        Viewport3D* vp = (Viewport3D*)TypeLeaf(rootViewport, 1);
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
        if (!vp || !o) { err = "viewlocal: sin Vista 3D o sin objeto '" + name + "'"; return false; }
        Frame();
        if (!vp->localViewActive) { err = "vistalocal: la Vista 3D no esta en view local"; return false; }
        const int has = vp->localViewSet.count(o) ? 1 : 0;
        printf("      [vistalocal] '%s' %s en la view local (esperaba %d)\n", name.c_str(), has ? "se ve" : "NO se ve", v);
        if (has != v) { err = "viewlocal: '" + name + "' no quedo como se esperaba en la view local"; return false; }
        return true;
    }
    // 'pickvertex abajo|arriba': en el Modo Edicion, elige SOLO el vertice mas bajo (o mas alto) de la malla, como
    // un click sobre el (el polo de abajo de una esfera: ahi va el cursor 3D para aplastarla desde el piso)
    if (cmd == "pickvertex") {
        std::string q; ss >> q;
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "pickvertex: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        if (!e || e->NumVerts() <= 0) { err = "pickvertex: la malla no tiene vertices"; return false; }
        int best = 0;
        for (int k = 1; k < e->NumVerts(); k++) {
            const float y = e->pos[(size_t)k * 3 + 1], yb = e->pos[(size_t)best * 3 + 1];
            if (q == "up" ? y > yb : y < yb) best = k;
        }
        EditSelectMode = SelVertex;
        e->SelectAll(false);
        e->ToggleVert(best, true);
        g_redraw = true; Frame();
        return true;
    }
    // 'picknear <x> <y> <z>': en el Modo Edicion elige SOLO el elemento del modo actual (vertice, arista o cara) mas
    // cercano a ese punto de la malla (en local, como Propiedades: Z arriba). Es el click de la persona sobre "la cara de
    // adelante" o "la arista de afuera", sin depender de la camara
    if (cmd == "picknear") {
        float ux = 0, uy = 0, uz = 0; std::string add; ss >> ux >> uy >> uz >> add;
        const bool sum = (add == "+");           // '+': se suma a lo que ya estaba elegido (Shift + click)
        const float px = ux, py = uz, pz = uy;   // Z arriba -> interno (Y arriba)
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "picknear: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        if (!e) { err = "picknear: sin edit mesh"; return false; }
        int best = -1; float dBest = 1e30f;
        if (EditSelectMode == SelFace) {
            for (int f = 0; f < e->NumFaces(); f++) {
                const float* c = &e->faceCenter[(size_t)f * 3];
                const float d = (c[0]-px)*(c[0]-px) + (c[1]-py)*(c[1]-py) + (c[2]-pz)*(c[2]-pz);
                if (d < dBest) { dBest = d; best = f; }
            }
            if (!sum) e->SelectAll(false); if (best >= 0) e->ToggleFace(best, !sum);
        } else if (EditSelectMode == SelEdge) {
            for (int a = 0; a < e->NumEdges(); a++) {
                const int k0 = e->lineIdx[(size_t)a * 2], k1 = e->lineIdx[(size_t)a * 2 + 1];
                const float* p0 = &e->pos[(size_t)k0 * 3]; const float* p1 = &e->pos[(size_t)k1 * 3];
                const float cx = (p0[0]+p1[0])*0.5f, cy = (p0[1]+p1[1])*0.5f, cz = (p0[2]+p1[2])*0.5f;
                const float d = (cx-px)*(cx-px) + (cy-py)*(cy-py) + (cz-pz)*(cz-pz);
                if (d < dBest) { dBest = d; best = a; }
            }
            if (!sum) e->SelectAll(false); if (best >= 0) e->ToggleEdge(best, !sum);
        } else {
            for (int k = 0; k < e->NumVerts(); k++) {
                const float* q = &e->pos[(size_t)k * 3];
                const float d = (q[0]-px)*(q[0]-px) + (q[1]-py)*(q[1]-py) + (q[2]-pz)*(q[2]-pz);
                if (d < dBest) { dBest = d; best = k; }
            }
            if (!sum) e->SelectAll(false); if (best >= 0) e->ToggleVert(best, !sum);
        }
        if (best < 0) { err = "picknear: no hay elementos"; return false; }
        printf("      [elegircerca] elemento %d a %.4f m\n", best, sqrtf(dBest));
        g_redraw = true; Frame();
        return true;
    }
    // 'linked': agranda lo elegido a toda la pieza conectada (Seleccionar Vinculado, la L)
    if (cmd == "linked") {
        Mesh* m = (Mesh*)g_editMesh;
        if (!m) { err = "linked: no hay una malla en Modo Edicion"; return false; }
        m->EnsureEdit(); EditMesh* e = m->edit;
        int k = -1;
        for (int i = 0; i < e->NumVerts() && k < 0; i++) if (i < (int)e->vertSel.size() && e->vertSel[i]) k = i;
        if (k < 0) for (int f = 0; f < e->NumFaces() && k < 0; f++) if (f < (int)e->faceSel.size() && e->faceSel[f] && !e->faces[f].empty()) k = e->faces[f][0];
        if (k < 0) for (int a = 0; a < e->NumEdges() && k < 0; a++) if (a < (int)e->edgeSel.size() && e->edgeSel[a]) k = e->lineIdx[(size_t)a * 2];
        if (k < 0) { err = "linked: no hay nada elegido"; return false; }
        e->SelectLinked(k, EditSelectMode, false);
        g_redraw = true; Frame();
        return true;
    }
    // 'proxyw3d <archivo.w3d>': Add > Proxy W3D como si el explorador hubiera elegido ese .w3d (queda abierto el .w3d por
    // dentro). 'fbenter <nombre>': en el explorador abierto, entra a esa carpeta o elige ese archivo. 'fbaccept': el
    // boton verde. 'fbshow': lo que muestra (la ruta, las entradas y el boton)
    if (cmd == "proxyw3d") {
        std::string r; ss >> r;
        extern void LayoutProxyW3dFrom(const std::string&);
        LayoutProxyW3dFrom(r);
        if (!PopUpActive) { err = "proxyw3d: no se abrio el explorador de la libreria"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    if (cmd == "fbenter" || cmd == "fbaccept" || cmd == "fbshow") {
        FileBrowser* fb = dynamic_cast<FileBrowser*>(PopUpActive);
        if (!fb) { err = cmd + ": no hay un explorador abierto"; return false; }
        if (cmd == "fbenter") { const std::string n = Rest(ss); if (!fb->EnterName(n)) { err = "fbenter: no hay '" + n + "'"; return false; } }
        else if (cmd == "fbaccept") fb->AcceptNow();
        else {
            printf("      [fbver] '%s':", fb->currentPath.c_str());
            for (size_t i = 0; i < fb->entries.size(); i++) printf(" [%s%s]", fb->entries[i].name.c_str(), fb->entries[i].isDir ? "/" : "");
            printf(" -> boton '%s'\n", fb->LabelAction().c_str());
        }
        g_redraw = true; Frame();
        return true;
    }
    // 'minimemory <KB>': la RAM libre que las vistas previas de la biblioteca dejan (con una enorme: "no hay memoria")
    if (cmd == "minimemory") {
        long kb = 0; ss >> kb;
        extern void W3dThumbnailsMinKB(long);
        W3dThumbnailsMinKB(kb);
        return true;
    }
    // 'menukeyboard <3d|outliner|props|timeline> [abajos]': el OK MANTENIDO del N95 en ese viewport (lo hace activo): el
    // menu contextual de su elemento con foco (antes, 'abajos' veces la flecha abajo del N95). Imprime sus opciones
    if (cmd == "menukeyboard") {
        std::string q; int downs = 0; ss >> q >> downs;
        const int kind = q == "3d" ? 1 : q == "outliner" ? 2 : q == "props" ? 3 : q == "timeline" ? 5 : q == "library" ? 9 : -1;
        if (kind < 0) { err = "menukeyboard: 3d|outliner|biblioteca|props|timeline"; return false; }
        ViewportBase* vp = TypeLeaf(rootViewport, kind);
        if (!vp) { err = "menukeyboard: no hay ese viewport en el layout"; return false; }
        viewPortActive = vp;
        { extern bool LayoutKeyPanelActive(int key); for (int i = 0; i < downs; i++) LayoutKeyPanelActive(1); }
        extern bool LayoutMenuContextKeyboard();
        extern PopupMenu* OpenMenu;
        if (!LayoutMenuContextKeyboard() || !OpenMenu) { err = "menukeyboard: no se abrio ningun menu"; return false; }
        printf("      [menuteclado] %s: '%s' en %d,%d:", q.c_str(), OpenMenu->title.c_str(), OpenMenu->x, OpenMenu->y);
        for (size_t i = 0; i < OpenMenu->items.size(); i++) printf(" [%s]", OpenMenu->items[i]->text.c_str());
        printf("\n");
        g_redraw = true; Frame();
        return true;
    }
    // 'menupick <texto en ingles>': elige esa opcion del menu abierto, como Enter sobre ella (Shift+S y despues
    // "Cursor to Selected")
    if (cmd == "menupick") {
        const std::string t = T(Rest(ss).c_str());
        extern PopupMenu* OpenMenu;
        PopupMenu* m = (OpenMenu && OpenMenu->open) ? OpenMenu : NULL;
        int row = -1;
        if (m) for (size_t i = 0; i < m->items.size(); i++) if (m->items[i]->text == t) row = (int)i;
        if (row < 0) {
            err = "menupick: " + std::string(m ? "el menu abierto no tiene '" + t + "' (tiene:" : "no hay ningun menu abierto");
            if (m) { for (size_t i = 0; i < m->items.size(); i++) err += " '" + m->items[i]->text + "'"; err += ")"; }
            return false;
        }
        m->selectIndex = row;
        const int id = m->Enter();
        if (id >= 0) m->Run(id);
        g_redraw = true; Frame();
        return true;
    }
    // 'emptyset <forma> <tamanio> <rx> <ry> <rz>': prepara el dibujo del Vacio activo (para armar una escena, como
    // 'cursor3d' u 'origin'; lo que hace la persona se prueba con propclic + menuelegir)
    if (cmd == "emptyset") {
        int fo = 2; float t = 1, rx = 0, ry = 0, rz = 0; ss >> fo >> t >> rx >> ry >> rz;
        if (!ActiveObj || ActiveObj->getType() != ObjectType::empty) { err = "emptyset: el activo no es un Vacio"; return false; }
        Empty* e = (Empty*)ActiveObj;
        if (fo >= 0 && fo < EmptyShapes) e->shape = fo;
        e->drawingSize = t; e->rotDrawing = Vector3(rx, ry, rz);
        g_redraw = true; Frame();
        return true;
    }
    // 'emptydraw <forma> <tamanio> <rx> <ry> <rz>': el dibujo del Vacio activo (forma: 0 cruz, 1 cuadrado, 2 circulo,
    // 3 esfera). Lo que se elige en Propiedades y lo que vuelve al abrir el .w3d
    if (cmd == "emptydraw") {
        int fo = -1; float t = 0, rx = 0, ry = 0, rz = 0; ss >> fo >> t >> rx >> ry >> rz;
        if (!ActiveObj || ActiveObj->getType() != ObjectType::empty) { err = "emptydraw: el activo no es un Vacio"; return false; }
        const Empty* e = (const Empty*)ActiveObj;
        char b[200];
        snprintf(b, sizeof b, "emptydraw '%s': forma %d, tamanio %.3f, giro %.1f %.1f %.1f (esperaba %d, %.3f, %.1f %.1f %.1f)",
                 e->name.c_str(), e->shape, e->drawingSize, e->rotDrawing.x, e->rotDrawing.y, e->rotDrawing.z, fo, t, rx, ry, rz);
        printf("      %s\n", b);
        if (e->shape != fo || fabsf(e->drawingSize - t) > 1e-3f || fabsf(e->rotDrawing.x - rx) > 0.05f || fabsf(e->rotDrawing.y - ry) > 0.05f ||
            fabsf(e->rotDrawing.z - rz) > 0.05f) { err = b; return false; }
        return true;
    }
    // 'keyobj <frame> <x>': el objeto activo en X = x (interno) y un keyframe de todo en ese cuadro (la I)
    if (cmd == "keyobj") {
        int f = 1; float px = 0; ss >> f >> px;
        if (!ActiveObj) { err = "keyobj: sin objeto activo"; return false; }
        extern int CurrentFrame; extern void InsertKeyframeObject(int);
        CurrentFrame = f; ActiveObj->pos.x = px; InsertKeyframeObject(0);
        g_redraw = true; Frame();
        return true;
    }
    // 'curvepoints <tipo> <n>': la forma de una curva (la MISMA cuenta del motor, KfEasing): n+1 puntos t -> fraccion
    // del camino. Para dibujarla al lado de la pelota que la usa
    if (cmd == "curvepoints") {
        std::string q; int n = 32; ss >> q >> n;
        const int v = InterpByName(q);
        if (v < 0 || n < 2) { err = "curvepoints: uso: curvepoints <tipo> <n>"; return false; }
        for (int i = 0; i <= n; i++) {
            const float t = (float)i / (float)n;
            const float y = (v == KfLinear) ? t : (v == KfBezier) ? t * t * (3.0f - 2.0f * t) : KfEasing(v, t);
            printf("      [curvapuntos] %.5f %.5f\n", t, y);
        }
        return true;
    }
    // 'keyset <frame> <loc|esc> <x> <y> <z>': el objeto activo con esa posicion o esa escala (como Propiedades: Z
    // arriba) y un keyframe SOLO de eso en ese cuadro (I > Solo Localizacion / Solo Escala). Para armar una demo
    if (cmd == "keyset") {
        int f = 1; std::string q; float ux = 0, uy = 0, uz = 0; ss >> f >> q >> ux >> uy >> uz;
        if (!ActiveObj) { err = "keyset: sin objeto activo"; return false; }
        extern int CurrentFrame; extern void InsertKeyframeObject(int);
        CurrentFrame = f;
        if (q == "loc") { ActiveObj->pos.x = ux; ActiveObj->pos.y = uz; ActiveObj->pos.z = uy; InsertKeyframeObject(KfChannelLoc); }
        else if (q == "esc") { ActiveObj->scale.x = ux; ActiveObj->scale.y = uz; ActiveObj->scale.z = uy; InsertKeyframeObject(KfChannelScl); }
        else { err = "keyset: uso: keyset <frame> loc|esc <x> <y> <z>"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'curvemod <loc|rot|esc> <x|y|z> <ciclos|ruido|escalon|suave> [campo valor]...': agrega un MODIFICADOR DE CURVA
    // (Animation.h, KfModifier) a esa curva del objeto activo (Z arriba, como Propiedades), lo mismo que Agregar en la
    // tarjeta "Modificadores de curva". Campos: fuerza, escala, fase, profundidad, paso, ancho, sigma, influencia y
    // antes / despues (los ciclos: 0 sin ciclos, 1 repetir, 2 con desplazamiento, 3 en espejo)
    if (cmd == "curvemod") {
        std::string q, e, t; ss >> q >> e >> t;
        if (!ActiveObj) { err = "curvemod: sin objeto activo"; return false; }
        const int prop = (q == "loc") ? AnimPosition : (q == "rot") ? AnimRotation : (q == "esc") ? AnimScale : -1;
        const int comp = (e == "x") ? AnimX : (e == "y") ? AnimZ : (e == "z") ? AnimY : -1;   // (Y de la UI = z interno)
        const int type = (t == "cycles") ? KfModCycles : (t == "noise") ? KfModNoise : (t == "step") ? KfModStep
                       : (t == "smooth") ? KfModSmooth : -1;
        if (prop < 0 || comp < 0 || type < 0) { err = "curvemod: uso: curvemod <loc|rot|esc> <x|y|z> <ciclos|ruido|escalon|suave> [campo valor]..."; return false; }
        AnimProperty* ap = NULL;
        for (size_t i = 0; i < AnimationObjects.size() && !ap; i++) {
            if (AnimationObjects[i].obj != ActiveObj) continue;
            for (size_t k = 0; k < AnimationObjects[i].Properties.size(); k++) {
                AnimProperty& c = AnimationObjects[i].Properties[k];
                if (c.Property == prop && c.component == comp) { ap = &c; break; }
            }
        }
        if (!ap) { err = "curvemod: el objeto activo no tiene esa curva (keyframes primero)"; return false; }
        KfModifier m(type);
        std::string field; float v = 0;
        while (ss >> field >> v) {
            if (field == "force") m.force = v; else if (field == "scale") m.scale = v; else if (field == "phase") m.phase = v;
            else if (field == "depth") m.depth = v; else if (field == "step") m.step = v; else if (field == "width") m.width = v;
            else if (field == "sigma") m.sigma = v; else if (field == "before") m.prevMode = (int)v; else if (field == "after") m.afterMode = (int)v;
            else if (field == "influence") { m.useInfluence = true; m.influence = v; }
            else { err = "curvemod: campo desconocido '" + field + "'"; return false; }
        }
        ap->mods.push_back(m);
        extern void ApplyAnimationObjects(); ApplyAnimationObjects();
        g_redraw = true; Frame();
        return true;
    }
    // 'keyinterp <lineal|bezier|in|out|inout> [frame] [loc|rot|esc]': la CURVA de los keyframes del objeto activo (Key >
    // Interpolacion / Easing): lineal (robotico), bezier, o cubica que acelera al arrancar (in), frena al llegar (out) o
    // las dos (inout). Con un cuadro, solo el tramo que SALE de ese cuadro; con loc/rot/esc, solo esas curvas
    // 'scriptval "<objeto>" <tarjeta> <prop> <valor>': lo MISMO que el panel de Propiedades al cambiar un valor de un
    // script (ScriptValSave): lo guarda en el objeto y, con el juego andando, se lo avisa a la sim (SimReresolve)
    // para que el script lo vea en ese momento, como el inspector de Unity
    if (cmd == "scriptval") {
        std::string rest; std::getline(ss, rest);
        size_t a = rest.find('"'), b2 = (a == std::string::npos) ? a : rest.find('"', a + 1);
        if (b2 == std::string::npos) { err = "scriptval: el objeto va entre comillas"; return false; }
        const std::string name = rest.substr(a + 1, b2 - a - 1);
        std::istringstream r2(rest.substr(b2 + 1));
        int card = 0; std::string prop, value; r2 >> card >> prop >> value;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
        if (!o || !o->scriptData || card < 0 || card >= (int)o->scriptData->scripts.size()) {
            err = "scriptval: '" + name + "' no tiene la tarjeta de script " + std::to_string(card); return false;
        }
        W3dScriptEntry& e = o->scriptData->scripts[card];
        bool has = false;
        for (size_t i = 0; i < e.refs.size(); i++) if (e.refs[i].first == prop) { e.refs[i].second = value; has = true; }
        if (!has) e.refs.push_back(std::make_pair(prop, value));
        extern bool SimActive(); extern void SimReresolve(Object*);
        if (SimActive()) SimReresolve(o);
        printf("      [scriptval] '%s' %s = %s%s\n", name.c_str(), prop.c_str(), value.c_str(), SimActive() ? " (en vivo)" : "");
        g_redraw = true; Frame();
        return true;
    }
    // 'scriptref "<objeto>" <tarjeta> <prop> [esperado]': el valor guardado de una propiedad de ese script (lo del
    // inspector); '-' = no tiene valor propio (usa el de fabrica). Con 'esperado', falla si no es ese
    if (cmd == "scriptref") {
        std::string rest; std::getline(ss, rest);
        size_t a = rest.find('"'), b2 = (a == std::string::npos) ? a : rest.find('"', a + 1);
        if (b2 == std::string::npos) { err = "scriptref: el objeto va entre comillas"; return false; }
        const std::string name = rest.substr(a + 1, b2 - a - 1);
        std::istringstream r2(rest.substr(b2 + 1));
        int card = 0; std::string prop, expected; r2 >> card >> prop >> expected;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, name) : NULL;
        if (!o || !o->scriptData || card < 0 || card >= (int)o->scriptData->scripts.size()) {
            err = "scriptref: '" + name + "' no tiene la tarjeta de script " + std::to_string(card); return false;
        }
        std::string has = "-";
        const W3dScriptEntry& e = o->scriptData->scripts[card];
        for (size_t i = 0; i < e.refs.size(); i++) if (e.refs[i].first == prop) has = e.refs[i].second;
        printf("      [scriptref] '%s' [%d] %s = %s\n", name.c_str(), card, prop.c_str(), has.c_str());
        if (!expected.empty() && has != expected) { err = "scriptref: se esperaba " + expected; return false; }
        return true;
    }
    if (cmd == "keyinterp") {
        std::string q, qp; int fsolo = -1; ss >> q; if (!(ss >> fsolo)) fsolo = -1; ss >> qp;
        const int psolo = (qp == "loc") ? AnimPosition : (qp == "rot") ? AnimRotation : (qp == "esc") ? AnimScale : -1;
        const int v = InterpByName(q);
        if (v < 0) { err = "keyinterp: lineal|bezier|in|out|inout|cuadratica|rebote|elastica [frame] [loc|rot|esc]"; return false; }
        int n = 0;
        for (size_t a = 0; a < AnimationObjects.size(); a++) {
            if (AnimationObjects[a].obj != ActiveObj) continue;
            for (size_t p = 0; p < AnimationObjects[a].Properties.size(); p++) {
                if (psolo >= 0 && AnimationObjects[a].Properties[p].Property != psolo) continue;
                for (size_t k = 0; k < AnimationObjects[a].Properties[p].keyframes.size(); k++) {
                    if (fsolo >= 0 && AnimationObjects[a].Properties[p].keyframes[k].frame != fsolo) continue;
                    AnimationObjects[a].Properties[p].keyframes[k].Interpolation = v; n++;
                }
            }
        }
        if (!n) { err = "keyinterp: el objeto activo no tiene keyframes"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'tl selframe <f> | duplicar | copiar | pegar | valor <texto> | aceptar | cancelar | frame <f>': la LINEA DE TIEMPO
    // por sus funciones de siempre (las de Shift+D, Ctrl+C, Ctrl+V, el numero tipeado mientras se mueve, Enter, Esc)
    if (cmd == "tl") {
        std::string q; ss >> q;
        Timeline* tl = (Timeline*)TypeLeaf(rootViewport, 5);
        if (!tl) { err = "tl: no hay una Linea de tiempo"; return false; }
        extern int CurrentFrame;
        tl->BuildDopeRows();
        if (q == "selframe") { int f = 0; ss >> f; tl->DopeSelectNone(); tl->DopeSelFrame(f, true); }
        else if (q == "duplicate") tl->DopeDuplicateSelection();
        else if (q == "copy") tl->DopeCopySelection();
        else if (q == "paste") tl->DopePaste();
        else if (q == "value") { std::string t; ss >> t; for (size_t i = 0; i < t.size(); i++) tl->DopeNumChar((unsigned char)t[i]); tl->DopeNumApply(); }
        else if (q == "accept") tl->DopeMoveConfirm();
        else if (q == "cancel") tl->DopeMoveCancel();
        else if (q == "frame") { int f = 0; ss >> f; CurrentFrame = f; }
        else if (q == "curves") tl->ChangeMode(true);    // el EDITOR DE CURVAS (View > Curvas)
        else if (q == "dope") tl->ChangeMode(false);
        else if (q == "end") { int f = 0; ss >> f; tl->fEnd = (float)f; tl->ApplyEnd(); }   // el Fin del reloj
        else if (q == "focus") {   // el editor de curvas encuadra TODAS sus curvas (tiempo y valor), como View
            float f0 = 0, f1 = 0, v0 = 0, v1 = 0;
            if (tl->CurveViewRange(false, f0, f1, v0, v1)) tl->CurveFrame(f0, f1, v0, v1);
        }
        else if (q == "view") {   // 'tl vista <frame izq> <frame der> [valor abajo] [valor arriba]': el encuadre
            float f0 = 0, f1 = 30, v0 = 0, v1 = 0; ss >> f0 >> f1; const bool withV = (bool)(ss >> v0 >> v1);
            const float widthStrip = (float)(tl->width - tl->panelW);
            const float zf = widthStrip / ((f1 - f0) > 0.5f ? (f1 - f0) : 0.5f);
            float zv = tl->pxPerUnit, cv = tl->viewCenterV;
            if (withV && v1 > v0) { zv = (float)(tl->height - tl->stripY) / (v1 - v0); cv = (v0 + v1) * 0.5f; }
            tl->ViewApply(zf, f0, zv, cv);
        }
        else if (q == "deletecurve") {   // 'tl borrarcurva <loc|rot|esc> <x|y|z>' (eje como Propiedades: Z arriba)
            std::string qp, qe; ss >> qp >> qe;
            const int p = (qp == "loc") ? AnimPosition : (qp == "rot") ? AnimRotation : (qp == "esc") ? AnimScale : -1;
            const int c = (qe == "x") ? AnimX : (qe == "y") ? AnimZ : (qe == "z") ? AnimY : -1;   // Z arriba -> Y interno
            int n = 0;
            for (size_t a = 0; a < AnimationObjects.size(); a++) {
                if (AnimationObjects[a].obj != ActiveObj) continue;
                for (size_t k = 0; k < AnimationObjects[a].Properties.size(); k++)
                    if (AnimationObjects[a].Properties[k].Property == p && AnimationObjects[a].Properties[k].component == c) {
                        AnimationObjects[a].Properties[k].keyframes.clear(); n++;
                    }
            }
            if (!n) { err = "tl borrarcurva: el objeto activo no tiene esa curva"; return false; }
        }
        else { err = "tl: uso: tl selframe <f>|duplicar|copiar|pegar|valor <texto>|aceptar|cancelar|frame <f>|fin <f>|borrarcurva <p> <eje>"; return false; }
        g_redraw = true; Frame();
        return true;
    }
    // 'keyvalue <loc|rot|esc> <x|y|z> <cuadro> <valor> [interp]': el objeto activo tiene un keyframe en ese cuadro de
    // esa curva, con ese valor (+-0.001) y, si se pasa, esa interpolacion (el int tal cual: 2 bezier, 261 cubica al
    // arrancar, 517 cubica al llegar). El eje es el de la Vista 3D (Z arriba: S Z escala lo vertical). Lo que deja un
    // tutorial de animacion
    if (cmd == "keyvalue") {
        std::string qp, qe; int f = 0; float val = 0; int interp = -1; ss >> qp >> qe >> f >> val; if (!(ss >> interp)) interp = -1;
        if (!ActiveObj) { err = "keyvalue: sin objeto activo"; return false; }
        const int p = (qp == "loc") ? AnimPosition : (qp == "rot") ? AnimRotation : (qp == "esc") ? AnimScale : -1;
        const int c = (qe == "x") ? AnimX : (qe == "y") ? AnimZ : (qe == "z") ? AnimY : -1;   // Z arriba -> Y interno
        const keyFrame* kf = NULL;
        for (size_t a = 0; a < AnimationObjects.size() && !kf; a++) {
            if (AnimationObjects[a].obj != ActiveObj) continue;
            for (size_t k = 0; k < AnimationObjects[a].Properties.size() && !kf; k++) {
                const AnimProperty& ap = AnimationObjects[a].Properties[k];
                if (ap.Property != p || ap.component != c) continue;
                for (size_t i = 0; i < ap.keyframes.size(); i++) if (ap.keyframes[i].frame == f) { kf = &ap.keyframes[i]; break; }
            }
        }
        char b[200];
        if (!kf) { snprintf(b, sizeof b, "keyvalue: no hay keyframe de %s %s en el cuadro %d", qp.c_str(), qe.c_str(), f); err = b; return false; }
        snprintf(b, sizeof b, "keyvalue %s %s @%d = %.4f/%d (esperaba %.4f/%d)", qp.c_str(), qe.c_str(), f, kf->value, kf->Interpolation, val, interp);
        printf("      %s\n", b);
        if (fabsf(kf->value - val) > 0.001f || (interp >= 0 && kf->Interpolation != interp)) { err = b; return false; }
        return true;
    }
    // 'keyshow': imprime las curvas del objeto activo (propiedad, eje interno, y cada keyframe: cuadro=valor/interp).
    // Para comparar lo que deja el tutorial con el .w3d de la persona
    if (cmd == "keyshow") {
        if (!ActiveObj) { err = "keyshow: sin objeto activo"; return false; }
        for (size_t a = 0; a < AnimationObjects.size(); a++) {
            if (AnimationObjects[a].obj != ActiveObj) continue;
            for (size_t k = 0; k < AnimationObjects[a].Properties.size(); k++) {
                const AnimProperty& ap = AnimationObjects[a].Properties[k];
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
        std::string nm; int expected = -1; ss >> nm >> expected;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o) { err = "objkeys: no hay un objeto '" + nm + "'"; return false; }
        struct K { static int Of(const std::vector<AnimationObject>& v, Object* o) {
            int n = 0;
            for (size_t a = 0; a < v.size(); a++)
                if (v[a].obj == o)
                    for (size_t k = 0; k < v[a].Properties.size(); k++) n += (int)v[a].Properties[k].keyframes.size();
            return n;
        } };
        int n = K::Of(AnimationObjects, o);
        for (size_t e = 0; e < SceneAnimations.size(); e++)
            if ((int)e != ActiveSceneAnim && SceneAnimations[e]) n += K::Of(SceneAnimations[e]->objects, o);
        printf("      [objkeys] %s: %d keyframe(s) de escena (%d animacion(es) de escena)\n", nm.c_str(), n,
               (int)SceneAnimations.size());
        if (expected >= 0 && n != expected) {
            err = "objkeys: " + nm + " tiene " + std::to_string(n) + " keyframe(s), se esperaban " + std::to_string(expected);
            return false;
        }
        return true;
    }
    // 'objscript <objeto> <fila> [ruta R] [apagado 0|1] [ref P V]...': el script de esa fila de un objeto de la raiz
    // activa: su .lua, su ojo y sus valores (los asserta)
    if (cmd == "objscript") {
        std::string nm; int row = -1; ss >> nm >> row;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o || !o->scriptData || row < 0 || row >= (int)o->scriptData->scripts.size()) {
            err = "objscript: '" + nm + "' no tiene el script " + std::to_string(row); return false;
        }
        const W3dScriptEntry& e = o->scriptData->scripts[(size_t)row];
        printf("      [objscript] %s #%d: %s%s, %d valor(es)\n", nm.c_str(), row, e.path.c_str(), e.off ? " (APAGADO)" : "",
               (int)e.refs.size());
        std::string k;
        while (ss >> k) {
            if (k == "path") {
                std::string r; ss >> r;
                if (e.path != r) { err = "objscript: el script es '" + e.path + "', se esperaba '" + r + "'"; return false; }
            } else if (k == "off") {
                int a = -1; ss >> a;
                if ((e.off ? 1 : 0) != a) { err = std::string("objscript: el script esta ") + (e.off ? "off" : "on"); return false; }
            } else if (k == "ref") {
                std::string p, v; ss >> p >> v;
                bool has = false;
                for (size_t r = 0; r < e.refs.size(); r++)
                    if (e.refs[r].first == p) {
                        has = true;
                        if (e.refs[r].second != v) { err = "objscript: " + p + " = '" + e.refs[r].second + "', se esperaba '" + v + "'"; return false; }
                    }
                if (!has) { err = "objscript: el script no tiene el valor '" + p + "'"; return false; }
            } else { err = "objscript: no entiendo '" + k + "'"; return false; }
        }
        return true;
    }
    // 'objphysics <objeto> poner <estatico|dinamico|personaje|malla> <masa>': le pone un cuerpo rigido (lo de la tarjeta
    // Fisica); 'objphysics <objeto> <tipo|-> [masa M]': asserta el que tiene ('-' = ninguno)
    if (cmd == "objphysics") {
        static const char* kTypes[4] = { "static", "dynamic", "character", "mesh" };
        std::string nm, t; ss >> nm >> t;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o) { err = "objphysics: no hay un objeto '" + nm + "'"; return false; }
        if (t == "set") {
            std::string q; float mass = 1.0f; ss >> q >> mass;
            int type = -1;
            for (int i = 0; i < 4; i++) if (q == kTypes[i]) type = i;
            if (type < 0) { err = "objphysics: uso: objphysics <objeto> poner estatico|dinamico|personaje|malla <masa>"; return false; }
            if (!o->physics) o->physics = new W3dRigidDef();
            o->physics->type = type; o->physics->mass = mass;
            return true;
        }
        const std::string has = o->physics ? std::string(kTypes[o->physics->type & 3]) : std::string("-");
        printf("      [objfisica] %s: %s", nm.c_str(), has.c_str());
        if (o->physics) printf(" masa %.3f", o->physics->mass);
        printf("\n");
        if (has != t) { err = "objfisica: " + nm + " tiene '" + has + "', se esperaba '" + t + "'"; return false; }
        std::string k;
        while (ss >> k) {
            float m = 0.0f; ss >> m;
            if (k == "mass" && o->physics && fabsf(o->physics->mass - m) > 1e-4f) { err = "objphysics: otra masa"; return false; }
        }
        return true;
    }
    // 'armaturestrip <armature> <clip> <frame>': una TIRA de esqueleto en la animacion de escena activa (el armature toca
    // ese clip desde ese frame de la escena: una cinematica)
    // 'armatureclip <armature> <clip|-> [cabezal F]': el clip ACTIVO del armature (el que reproduce en "Juego") y su
    // cabezal de juego (gameFrame: frames desde el inicio del clip)
    if (cmd == "armaturestrip" || cmd == "armatureclip") {
        std::string nm, clip; ss >> nm >> clip;
        Object* o = SceneCollection ? FindObjectByName(SceneCollection, nm) : NULL;
        if (!o || o->getType() != ObjectType::armature) { err = cmd + ": no hay un armature '" + nm + "'"; return false; }
        Armature* a = (Armature*)o;
        if (cmd == "armaturestrip") {
            int f = 1; ss >> f;
            InitSceneAnimations();
            if (ActiveSceneAnim < 0 || ActiveSceneAnim >= (int)SceneAnimations.size() || W3dArmatureClipByName(a, clip) < 0) {
                err = "armaturestrip: '" + nm + "' no tiene el clip '" + clip + "'"; return false;
            }
            SceneAnimation::SkeletonTrack p; p.armature = a->name; p.clip = clip; p.frame = f;
            SceneAnimations[(size_t)ActiveSceneAnim]->skeletons.push_back(p);
            return true;
        }
        const std::string has = (a->activeAnim >= 0 && a->activeAnim < (int)a->animations.size() && a->animations[(size_t)a->activeAnim])
                                    ? a->animations[(size_t)a->activeAnim]->name : std::string("-");
        printf("      [esqueletoclip] %s: clip %s cabezal %.3f\n", nm.c_str(), has.c_str(), a->gameFrame);
        if (has != clip) { err = "armatureclip: el clip activo de " + nm + " es '" + has + "', se esperaba '" + clip + "'"; return false; }
        std::string k; float v = 0.0f;
        if (ss >> k >> v && k == "playhead" && fabsf(a->gameFrame - v) > 1e-3f) {
            err = "armatureclip: el cabezal de " + nm + " es " + std::to_string(a->gameFrame) + ", se esperaba " + std::to_string(v);
            return false;
        }
        return true;
    }
    // 'tlframes <f1> <f2> ...': la Linea de tiempo tiene keyframes EXACTAMENTE en esos cuadros
    if (cmd == "tlframes") {
        Timeline* tl = (Timeline*)TypeLeaf(rootViewport, 5);
        if (!tl) { err = "tlframes: no hay una Linea de tiempo"; return false; }
        tl->BuildDopeRows();
        std::vector<int> has; tl->DopeFramesNav(has);
        std::vector<int> esp; int f; while (ss >> f) esp.push_back(f);
        std::string sh, se; char b[16];
        for (size_t i = 0; i < has.size(); i++) { snprintf(b, sizeof b, "%d ", has[i]); sh += b; }
        for (size_t i = 0; i < esp.size(); i++) { snprintf(b, sizeof b, "%d ", esp[i]); se += b; }
        printf("      [tlframes] hay: %s(esperaba: %s)\n", sh.c_str(), se.c_str());
        if (has != esp) { err = "tlframes: hay keyframes en " + sh + "y se esperaba " + se; return false; }
        return true;
    }
    // 'gizmomode <0|1|2>': el gizmo en mover / rotar / escalar
    if (cmd == "gizmomode") { int mo = 0; ss >> mo; GizmoSetMode(mo); g_redraw = true; Frame(); return true; }
    // 'fingergizmo <manija> <dx> <dy> <pasos>': el DEDO agarra esa manija del gizmo y la arrastra despacio, con la espera
    // de un toque de verdad. La camara NO se tiene que mover en ningun momento (antes orbitaba hasta pasar el umbral y
    // recien ahi giraba el objeto) y el objeto si tiene que cambiar
    if (cmd == "fingergizmo") {
        int man = -1, dx = 0, dy = 0, steps = 10; ss >> man >> dx >> dy >> steps;
        Viewport3D* vp = (Viewport3D*)TypeLeaf(rootViewport, 1);
        if (!vp || !ActiveObj) { err = "fingergizmo: sin Vista 3D o sin objeto activo"; return false; }
        Frame();
        float sx = 0, sy = 0;
        if (!GizmoHandleInScreen(vp, man, sx, sy)) { err = "fingergizmo: la manija no se ve"; return false; }
        const int x0 = vp->x + (int)(sx + 0.5f), y0 = vp->y + (int)(sy + 0.5f);
        const Vector3 cam0 = vp->viewPos;
        const Quaternion r0 = ActiveObj->Rot(); const Vector3 p0 = ActiveObj->pos, e0 = ActiveObj->scale;
        Moved(SDL_TOUCH_MOUSEID, 0, x0, y0, x0, y0); SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, true, x0, y0); SendFinger(SDL_FINGERDOWN, 1, x0, y0, x0, y0);
        SDL_Delay(250);   // (pasada la espera anti-gesto: un dedo que se queda un momento antes de arrastrar)
        int px = x0, py = y0; bool cameraStill = true;
        if (steps < 1) steps = 1;
        for (int k = 1; k <= steps; k++) {
            const int x = x0 + dx * k / steps, y = y0 + dy * k / steps;
            Moved(SDL_TOUCH_MOUSEID, SDL_BUTTON_LMASK, x, y, px, py); SendFinger(SDL_FINGERMOTION, 1, x, y, px, py);
            px = x; py = y;
            const Vector3 d = vp->viewPos - cam0;
            if (fabsf(d.x) + fabsf(d.y) + fabsf(d.z) > 1e-4f) cameraStill = false;
            SDL_Delay(15);
        }
        SendButton(SDL_TOUCH_MOUSEID, SDL_BUTTON_LEFT, false, px, py); SendFinger(SDL_FINGERUP, 1, px, py, px, py);
        const Quaternion r1 = ActiveObj->Rot();
        const bool change = fabsf(r1.w - r0.w) + fabsf(r1.x - r0.x) + fabsf(r1.y - r0.y) + fabsf(r1.z - r0.z) > 1e-4f ||
                            (ActiveObj->pos - p0).Length() > 1e-4f || (ActiveObj->scale - e0).Length() > 1e-4f;
        printf("      [dedogizmo] la camara %s, el objeto %s\n", cameraStill ? "still" : "SE MOVIO", change ? "change" : "NO cambio");
        if (!cameraStill) { err = "fingergizmo: la camara orbito mientras el dedo arrastraba el gizmo"; return false; }
        if (!change) { err = "fingergizmo: el objeto no cambio"; return false; }
        return true;
    }
    handled = false;
    return false;
}
