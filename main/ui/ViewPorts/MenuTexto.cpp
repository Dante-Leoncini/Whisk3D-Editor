// ============================================================================
//  MenuTexto.cpp — ver MenuTexto.h
// ============================================================================
#include "ViewPorts/MenuTexto.h"
#include "ViewPorts/IDE.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/TextField.h"
#include "WhiskUI/draw/icons.h"
#include "input/W3dClipboard.h"
#include "W3dLang.h"

extern bool g_redraw;
ViewportBase* FindViewportUnderMouse(ViewportBase* nodo, int mx, int my);
extern ViewportBase* rootViewport;

static PopupMenu* gMenu = NULL;
static IDE* gIde = NULL;             // NULL = el campo enfocado
static TextField* gCampo = NULL;     // el campo con el que se abrio (si el foco cambio, no se toca)
static bool gHaySel = false, gHayPegar = false, gHayTexto = false, gHayCortar = false;
enum { MT_CORTAR = 1, MT_COPIAR, MT_PEGAR, MT_TODO };

static void Elegido(int id) {
    if (gIde) {
        switch (id) {
            case MT_CORTAR: gIde->Cortar(); break;
            case MT_COPIAR: gIde->Copiar(); break;
            case MT_PEGAR:  gIde->Pegar(); break;
            case MT_TODO:   gIde->SeleccionarTodo(); break;
        }
        g_redraw = true;
        return;
    }
    if (!gCampo) return;
    if (g_textFieldActivo != gCampo) g_textFieldActivo = gCampo;   // (el foco vuelve al campo del menu)
    switch (id) {
        case MT_CORTAR: TextFieldCortar(); break;
        case MT_COPIAR: TextFieldCopiar(); break;
        case MT_PEGAR:  TextFieldPegar(); break;
        case MT_TODO:   TextFieldSeleccionarTodo(); break;
    }
    g_redraw = true;
}

static void Abrir(int x, int y) {
    if (!gMenu) { gMenu = new PopupMenu(); gMenu->action = Elegido; }
    gMenu->Limpiar();
    gHayPegar = gHayPegar && !w3dEngine::W3dClipboardGet().empty();
    MenuItem* it;
    it = gMenu->Agregar(T("Cut"), MT_CORTAR, (int)IconType::tijera);   if (it) it->gris = &gHayCortar;
    it = gMenu->Agregar(T("Copy"), MT_COPIAR, (int)IconType::copiar);  if (it) it->gris = &gHaySel;
    it = gMenu->Agregar(T("Paste"), MT_PEGAR, (int)IconType::pegar);   if (it) it->gris = &gHayPegar;
    it = gMenu->Agregar(T("Select All"), MT_TODO, (int)IconType::seleccion);
    if (it) { it->gris = &gHayTexto; it->lineaArriba = true; }
    if (MenuAbierto && MenuAbierto != gMenu) MenuAbierto->Cerrar();
    gMenu->Abrir(x, y, MenuPantallaW, MenuPantallaH);
    MenuAbierto = gMenu;
    g_redraw = true;
}

bool MenuTextoEnCampo(int mx, int my, int menuX, int menuY) {
    if (!g_textFieldActivo || !TextFieldPuntoEnActivo(mx, my)) return false;
    gIde = NULL; gCampo = g_textFieldActivo;
    gHaySel = gCampo->HaySeleccion();
    gHayTexto = !gCampo->text.empty();
    gHayCortar = gHaySel && !gCampo->soloLectura; gHayPegar = !gCampo->soloLectura;   // (solo lectura: copiar y nada mas)
    Abrir(menuX >= 0 ? menuX : mx, menuY >= 0 ? menuY : my);
    return true;
}

bool MenuTextoIDE(IDE* ide, int mx, int my) {
    if (!ide) return false;
    // el cursor va donde se toco, salvo que caiga ADENTRO de la seleccion (asi se copia / corta lo seleccionado)
    bool enSel = false;
    if (ide->haySel) {
        int lin = 0, col = 0; ide->PuntoATexto(mx, my, &lin, &col);
        int l0, c0, l1, c1; ide->NormalizarSel(&l0, &c0, &l1, &c1);
        enSel = (lin > l0 || (lin == l0 && col >= c0)) && (lin < l1 || (lin == l1 && col <= c1));
    }
    if (!enSel) ide->ClickContenido(mx, my, false);
    gIde = ide; gCampo = NULL;
    gHaySel = ide->haySel;
    gHayTexto = true;
    gHayCortar = gHaySel; gHayPegar = true;
    Abrir(mx, my);
    return true;
}

bool MenuTextoClickDerecho(int mx, int my) {
    if (MenuTextoEnCampo(mx, my, -1, -1)) return true;
    if (g_textFieldActivo || !rootViewport) return false;   // (con un campo enfocado el click derecho afuera es de otro)
    ViewportBase* vp = FindViewportUnderMouse(rootViewport, mx, my);
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 8) return false;
    if (vp->OnBar(mx, my) || vp->OnToolbar(mx, my)) return false;
    return MenuTextoIDE((IDE*)vp, mx, my);
}

bool MenuTextoAbierto() { return gMenu && MenuAbierto == gMenu && gMenu->abierto; }
