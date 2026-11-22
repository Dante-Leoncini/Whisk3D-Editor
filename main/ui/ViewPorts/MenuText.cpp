// ============================================================================
//  MenuText.cpp — ver MenuText.h
// ============================================================================
#include "ViewPorts/MenuText.h"
#include "ViewPorts/IDE.h"
#include "WhiskUI/widgets/PopupMenu.h"
#include "WhiskUI/widgets/TextField.h"
#include "WhiskUI/draw/icons.h"
#include "input/W3dClipboard.h"
#include "W3dLang.h"

extern bool g_redraw;
ViewportBase* FindViewportUnderMouse(ViewportBase* node, int mx, int my);
extern ViewportBase* rootViewport;

static PopupMenu* gMenu = NULL;
static IDE* gIde = NULL;             // NULL = el campo enfocado
static TextField* gField = NULL;     // el campo con el que se abrio (si el foco cambio, no se toca)
static bool gHasSel = false, gHasPaste = false, gHasText = false, gHasCut = false;
enum { MT_CUT = 1, MT_COPY, MT_PASTE, MT_ALL };

static void Picked(int id) {
    if (gIde) {
        switch (id) {
            case MT_CUT: gIde->Cut(); break;
            case MT_COPY: gIde->Copy(); break;
            case MT_PASTE:  gIde->Paste(); break;
            case MT_ALL:   gIde->SelectAll(); break;
        }
        g_redraw = true;
        return;
    }
    if (!gField) return;
    if (g_activeTextField != gField) g_activeTextField = gField;   // (el foco vuelve al campo del menu)
    switch (id) {
        case MT_CUT: TextFieldCut(); break;
        case MT_COPY: TextFieldCopy(); break;
        case MT_PASTE:  TextFieldPaste(); break;
        case MT_ALL:   TextFieldSelectAll(); break;
    }
    g_redraw = true;
}

static void Open(int x, int y) {
    if (!gMenu) { gMenu = new PopupMenu(); gMenu->action = Picked; }
    gMenu->Clear();
    gHasPaste = gHasPaste && !w3dEngine::W3dClipboardGet().empty();
    MenuItem* it;
    it = gMenu->Add(T("Cut"), MT_CUT, (int)IconType::scissors);   if (it) it->gray = &gHasCut;
    it = gMenu->Add(T("Copy"), MT_COPY, (int)IconType::copy);  if (it) it->gray = &gHasSel;
    it = gMenu->Add(T("Paste"), MT_PASTE, (int)IconType::paste);   if (it) it->gray = &gHasPaste;
    it = gMenu->Add(T("Select All"), MT_ALL, (int)IconType::selection);
    if (it) { it->gray = &gHasText; it->lineUp = true; }
    if (OpenMenu && OpenMenu != gMenu) OpenMenu->Close();
    gMenu->Open(x, y, MenuScreenW, MenuScreenH);
    OpenMenu = gMenu;
    g_redraw = true;
}

bool MenuTextInField(int mx, int my, int menuX, int menuY) {
    if (!g_activeTextField || !TextFieldPointInActive(mx, my)) return false;
    gIde = NULL; gField = g_activeTextField;
    gHasSel = gField->HasSelection();
    gHasText = !gField->text.empty();
    gHasCut = gHasSel && !gField->readOnly; gHasPaste = !gField->readOnly;   // (solo lectura: copiar y nada mas)
    Open(menuX >= 0 ? menuX : mx, menuY >= 0 ? menuY : my);
    return true;
}

bool MenuTextIDE(IDE* ide, int mx, int my) {
    if (!ide) return false;
    // el cursor va donde se toco, salvo que caiga ADENTRO de la seleccion (asi se copia / corta lo seleccionado)
    bool inSel = false;
    if (ide->hasSel) {
        int lin = 0, col = 0; ide->PointToText(mx, my, &lin, &col);
        int l0, c0, l1, c1; ide->NormalizeSel(&l0, &c0, &l1, &c1);
        inSel = (lin > l0 || (lin == l0 && col >= c0)) && (lin < l1 || (lin == l1 && col <= c1));
    }
    if (!inSel) ide->ClickContent(mx, my, false);
    gIde = ide; gField = NULL;
    gHasSel = ide->hasSel;
    gHasText = true;
    gHasCut = gHasSel; gHasPaste = true;
    Open(mx, my);
    return true;
}

bool MenuTextClickRight(int mx, int my) {
    if (MenuTextInField(mx, my, -1, -1)) return true;
    if (g_activeTextField || !rootViewport) return false;   // (con un campo enfocado el click derecho afuera es de otro)
    ViewportBase* vp = FindViewportUnderMouse(rootViewport, mx, my);
    if (!vp || !vp->isLeaf() || vp->ViewportKind() != 8) return false;
    if (vp->OnBar(mx, my) || vp->OnToolbar(mx, my)) return false;
    return MenuTextIDE((IDE*)vp, mx, my);
}

bool MenuTextOpen() { return gMenu && OpenMenu == gMenu && gMenu->open; }
