// ============================================================================
//  TimelineRedoPanel.cpp - "Ajustar la ultima operacion" del timeline (ver TimelineRedoPanel.h). C++03.
//  La interaccion (arrastrar un valor, tocarlo para tipearlo, desplazarse, plegar) es la de TransformRedoPanel; lo que
//  cambia es que se hizo y como se re-hace (TimelineRedoRead / TimelineRedoReapply, en Timeline.cpp).
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "TimelineRedoPanel.h"
#include "WhiskUI/Propieties/PropFloat.h"
#include "WhiskUI/Propieties/PropButton.h"
#include "WhiskUI/Propieties/PropButtonRow.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/Timeline.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "NumPad.h"
#include "Undo.h"
#include <math.h>

extern bool g_redraw;

static TimelineRedoPanel* gTL = NULL;
static TimelineRedoOp gTLop;
static float gTLv[2] = { 0, 0 };   // los dos valores editables (mover: frames/valor; rotar: angulo; escalar: X/Y)

bool TimelineRedoPanelOpen() { return gTL && PopUpActive == gTL; }
void TimelineRedoClosePanel() { if (TimelineRedoPanelOpen()) gTL->Close(); }
static void HookUndo() { if (!TimelineRedoReapplying()) { TimelineRedoForget(); TimelineRedoClosePanel(); } }   // Ctrl+Z / Ctrl+Y

static void ValuesToOp() {
    if (gTLop.mode == 0) { gTLop.frames = gTLv[0]; gTLop.value = gTLv[1]; }
    else if (gTLop.mode == 1) gTLop.angle = gTLv[0];
    else { gTLop.escX = gTLv[0]; gTLop.escY = gTLop.curves ? gTLv[1] : 1.0f; }
}
static void OnValue() { ValuesToOp(); TimelineRedoReapply(gTLop); }
static void Cancel_() { TimelineRedoCancel(); TimelineRedoClosePanel(); g_redraw = true; }
static void Accept_() { TimelineRedoForget(); TimelineRedoClosePanel(); }

void TimelineRedoOpen() {
    if (!TimelineRedoRead(gTLop)) return;
    if (gTLop.mode == 0) { gTLv[0] = gTLop.frames; gTLv[1] = gTLop.value; }
    else if (gTLop.mode == 1) { gTLv[0] = gTLop.angle; gTLv[1] = 0.0f; }
    else { gTLv[0] = gTLop.escX; gTLv[1] = gTLop.escY; }
    g_undoChangeHook = HookUndo;
    if (gTL) { if (PopUpActive == gTL) PopUpActive = NULL; delete gTL; gTL = NULL; }
    gTL = new TimelineRedoPanel();
    PopUpActive = gTL;
    g_redraw = true;
}

static const char* PivotName(int p) {
    switch (p) { case 1: return "Current Frame"; case 2: return "Median Point"; case 3: return "Active Keyframe"; default: return "Center"; }
}

TimelineRedoPanel::TimelineRedoPanel()
    : PopUpBase(""), group(NULL), dragField(NULL), dragMoved(false), lastDragMx(0), frame(NULL) {
    frame = new Card();
    Build();
}
TimelineRedoPanel::~TimelineRedoPanel() { delete group; delete frame; }

void TimelineRedoPanel::Build() {
    // nace PLEGADO (solo la flecha y el titulo, como la del 3D): no tapa el timeline si no se lo necesita
    const bool open = group ? group->open : false;
    delete group;
    const char* tit = (gTLop.mode == 1) ? "Rotate" : (gTLop.mode == 2) ? "Scale" : "Move";
    group = new GroupPropertie(T(tit));
    group->valueWidth = 0.55f;
    group->arrow = true;
    group->open = open;
    if (gTLop.mode == 0) {
        PropFloat* f = new PropFloat(T("Frames"));
        f->value = &gTLv[0]; f->SetRange(-1e6f, 1e6f); f->fineStep = 1.0f; f->thickStep = 10.0f; f->dragStep = 0.2f;
        f->onChange = OnValue;
        group->properties.push_back(f);
        if (gTLop.curves) {
            PropFloat* v = new PropFloat(T("Value"));
            v->value = &gTLv[1]; v->SetRange(-1e6f, 1e6f); v->fineStep = 0.1f; v->thickStep = 1.0f; v->dragStep = 0.01f;
            v->onChange = OnValue;
            group->properties.push_back(v);
        }
    } else if (gTLop.mode == 1) {
        PropFloat* a = new PropFloat(T("Angle"), "\xC2\xB0");
        a->value = &gTLv[0]; a->SetRange(-36000.0f, 36000.0f); a->fineStep = 1.0f; a->thickStep = 15.0f; a->dragStep = 0.5f;
        a->onChange = OnValue;
        group->properties.push_back(a);
    } else {
        // X = el TIEMPO, Y = el VALOR (solo en curvas: en el dope sheet no hay eje de valor)
        for (int k = 0; k < (gTLop.curves ? 2 : 1); k++) {
            PropFloat* f = new PropFloat(k == 0 ? "X" : "Y");
            f->value = &gTLv[k]; f->SetRange(-1e4f, 1e4f); f->fineStep = 0.05f; f->thickStep = 0.5f; f->dragStep = 0.005f;
            f->onChange = OnValue;
            group->properties.push_back(f);
        }
    }
    PropButton* p = new PropButton(T("Pivot"));
    p->withLabel = true; p->gray = true; p->button->text = T(PivotName(gTLop.pivot));
    group->properties.push_back(p);
    PropButtonRow* b = new PropButtonRow();
    b->Add(T("Cancel"), Cancel_);
    b->Add(T("Accept"), Accept_);
    b->active = 1;
    group->properties.push_back(b);
    group->selectIndex = 0;
    ResizeGroup();
    Relocate();
}
void TimelineRedoPanel::ResizeGroup() {
    Timeline* tl = TimelineRedoView();
    int w = 200 * GlobalScale;
    if (tl && w > tl->width - gapGS * 4) w = tl->width - gapGS * 4;
    if (w < 120 * GlobalScale) w = 120 * GlobalScale;
    group->Resize(w, 0);
    dz.Measure(group->height);
    popUpWindow->Resize(w, dz.visible);
}
void TimelineRedoPanel::Relocate() {   // abajo a la izquierda del timeline, encima de su barra de herramientas
    const int height = dz.visible;
    Timeline* tl = TimelineRedoView();
    if (!tl) { x = gapGS * 2; y = MenuScreenH - height - gapGS * 2; return; }
    x = tl->x + gapGS * 2;
    y = tl->y + tl->height - TimelineToolbarHeight(tl) - height - gapGS * 2;
    if (y < gapGS) y = gapGS;   // (un timeline mas bajo que la tarjeta: sube por encima de el)
    if (x < gapGS) x = gapGS;
}
int TimelineRedoPanel::RowY(int idx) const {
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int j = 0; j < idx && j < (int)group->properties.size(); j++) yRow += group->properties[j]->Resize(group->width);
    return yRow;
}

void TimelineRedoPanel::Render() {
    if (!group) return;
    ResizeGroup(); Relocate();
    initView();
    // ARRASTRANDO un valor: la tarjeta translucida (se ve detras el cambio) y la fila que se edita opaca, al final
    const int rowDrag = (dragField && dragMoved) ? group->IndexOf(dragField) : -1;
    if (rowDrag >= 0) w3dEngine::g_alphaColor = kAlphaCardDrag;
    w3dEngine::PushMatrix();
    w3dEngine::Translatef(0, (GLfloat)(-dz.scroll), 0);
    group->Render();
    w3dEngine::Translatef(0, (GLfloat)dz.scroll, 0);
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    frame->Resize(popUpWindow->width, dz.visible); frame->RenderBorder(false);
    if (dz.Scrollable()) {
        const int height = dz.visible * dz.visible / (dz.total > 0 ? dz.total : 1);
        const int ty = (dz.total - dz.visible > 0) ? (dz.visible - height) * dz.scroll / (dz.total - dz.visible) : 0;
        w3dEngine::Translatef((GLfloat)(popUpWindow->width - 3 * GlobalScale), (GLfloat)ty, 0);
        SetColorID(ColorID::accent, 0.7f);
        frame->Resize(2 * GlobalScale, height); frame->RenderObject(false);
    }
    w3dEngine::PopMatrix();
    w3dEngine::g_alphaColor = 1.0f;
    if (rowDrag >= 0) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(0, (GLfloat)(-dz.scroll), 0);   // (la misma base que el grupo)
        group->RenderRow(rowDrag);
        w3dEngine::PopMatrix();
    }
    endView();
}

bool TimelineRedoPanel::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera: el panel se cierra (acepta) y el click sigue al timeline
    const int myL = my - y + dz.scroll;
    if (myL < borderGS + RowHeightGS + gapGS) { group->open = !group->open; dz.scroll = 0; g_redraw = true; return true; }
    if (!group->open) return true;
    dz.Down(mx, my);
    if (!dz.Scrollable()) { dz.pressed = false; ClickRow(mx, my, false); return true; }
    ClickRow(mx, my, true);
    return true;
}
void TimelineRedoPanel::ClickRow(int mx, int my, bool onlyBuild) {
    const int myL = my - y + dz.scroll, lx = mx - x;
    const int boxStart = group->colLabel + 2 * borderGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h) {
            const PropertyType::Enum type = p->GetType();
            if (type == PropertyType::Float) {
                group->selectIndex = (int)j;
                g_activeTextField = NULL;
                dragField = static_cast<PropFloat*>(p); dragMoved = false; lastDragMx = mx;
                g_redraw = true;
                return;
            }
            if (onlyBuild) return;
            group->selectIndex = (int)j;
            g_activeTextField = NULL;
            if (type == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(p);
                const int cw = row->CellWidth(group->width), x0 = boxStart - group->colLabel;
                int cell = -1, vis = 0;
                for (size_t b = 0; b < row->buttons.size(); b++) {
                    if (!row->buttons[b]->visible) continue;
                    const int cx = x0 + vis * (cw + gapGS);
                    if (lx >= cx && lx < cx + cw) { cell = (int)b; break; }
                    vis++;
                }
                if (cell >= 0) { row->active = cell; if (row->actions[(size_t)cell]) row->actions[(size_t)cell](); }
                return;
            }
            g_redraw = true;
            return;
        }
        yRow += h;
    }
}
bool TimelineRedoPanel::Motion(int mx, int my) {
    if (!dragMoved && dz.Move(mx, my)) { dragField = NULL; g_redraw = true; return true; }
    if (dz.scrolling) return true;
    if (dragField && dragField->value) {
        if (dz.Scrollable() && !dragMoved) {
            const int threshold = 6 * (GlobalScale > 0 ? GlobalScale : 1);
            if (mx - dz.pressX < threshold && dz.pressX - mx < threshold) return true;
            lastDragMx = mx;
        }
        const int dmx = mx - lastDragMx; lastDragMx = mx;
        if (dmx != 0 || (dz.Scrollable() && !dragMoved)) dragMoved = true;
        if (dmx != 0) { dragField->Set(*dragField->value + dmx * dragField->dragStep); g_redraw = true; }
    }
    return true;
}
void TimelineRedoPanel::Release() {
    const bool touch = dz.Up();
    if (dragField) {
        if (!dragMoved && dragField->value) {
            dragField->StartEditText();            // mouse: se escribe el valor exacto
            if (ToolbarUsesTouch()) KeyboardNumberOpen();       // tactil: el teclado numerico en pantalla
        }
        dragField = NULL;
        return;
    }
    if (touch && dz.Scrollable()) ClickRow(dz.pressX, dz.pressY, false);
}
void TimelineRedoPanel::Wheel(int delta) { dz.Wheel(delta); g_redraw = true; }
bool TimelineRedoPanel::Dragging() { return dragField != NULL || dz.pressed; }
bool TimelineRedoPanel::Key(int key) {
    if (!group) return true;
    if (!group->open) {
        if (key == LayoutKey::Enter || key == LayoutKey::Right) { group->open = true; g_redraw = true; return true; }
        if (key == LayoutKey::Cancel) { Accept_(); return true; }
        return false;   // plegada: las demas teclas siguen de largo (se sigue trabajando)
    }
    const int n = (int)group->properties.size();
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {
            const int dir = (key == LayoutKey::Down) ? +1 : -1;
            int s = group->selectIndex; if (s < 0) s = (dir > 0) ? -1 : 0;
            for (int k = 0; k < n; k++) { s += dir; if (s < 0) s = n - 1; if (s >= n) s = 0; if (group->properties[s]->Selectable()) break; }
            group->selectIndex = s;
            dz.ToView(RowY(s), group->properties[(size_t)s]->Resize(group->width));
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (!sel) break;
            if (sel->GetType() == PropertyType::Float) { PropFloat* pf = static_cast<PropFloat*>(sel); pf->Set(*pf->value + (key == LayoutKey::Right ? pf->fineStep : -pf->fineStep)); }
            else if (sel->GetType() == PropertyType::ButtonRow) { PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->active += (key == LayoutKey::Right) ? 1 : -1; row->ActiveClamp(); }
            break;
        }
        case LayoutKey::Enter: {
            PropertieBase* sel = (group->selectIndex >= 0 && group->selectIndex < n) ? group->properties[group->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::ButtonRow) {
                PropButtonRow* row = static_cast<PropButtonRow*>(sel); row->ActiveClamp();
                if (row->active >= 0 && row->active < (int)row->actions.size() && row->actions[row->active]) row->actions[row->active]();
                return true;
            }
            if (sel && sel->GetType() == PropertyType::Float) static_cast<PropFloat*>(sel)->StartEditText();
            break;
        }
        case LayoutKey::Cancel: Accept_(); return true;   // Esc: cierra aceptando (solo Cancelar deshace)
    }
    g_redraw = true;
    return true;
}
void TimelineRedoPanel::Close() {
    g_activeTextField = NULL;
    PopUpBase::Close();
    g_redraw = true;
}

// (harness)
bool TimelineRedoSet(int field, float value) {
    if (!TimelineRedoPanelOpen() || field < 0 || field > 1) return false;
    gTLv[field] = value;
    ValuesToOp();
    return TimelineRedoReapply(gTLop);
}
void TimelineRedoButton(bool accept) { if (accept) Accept_(); else Cancel_(); }
