// ============================================================================
//  TimePopup.cpp - el desplegable del RELOJ del timeline (ver TimePopup.h). Misma mecanica que los formularios
//  (Export UV, Nueva textura): filas navegables con el teclado, flechas -1 / +1 en cada numero, arrastre para
//  correrlo y un click (sin arrastrar) para escribirlo. Coordenadas LOCALES al popup en el hit-test.
// ============================================================================
#include "w3dGraphics.h"
#include "W3dLang.h"
#include "TimePopup.h"
#include "WhiskUI/widgets/PopupMenu.h"      // MenuScreenW / MenuScreenH
#include "WhiskUI/widgets/card.h"
#include "ViewPorts/LayoutInput.h"          // LayoutKey
#include "ViewPorts/TransformUI.h"          // ToolbarUsesTouch
#include "NumPad.h"

extern bool g_redraw;

static TimePopup* gTP = NULL;

TimePopup::TimePopup(PropFloat* current, PropFloat* start, PropFloat* end, PropFloat* fps, int ax, int ay)
    : PopUpBase(T("Timing")), group(NULL), dragField(NULL), dragMoved(false), lastDragMx(0), anchorX(ax), anchorY(ay) {
    group = new GroupPropertie(T("Timing"));
    group->valueWidth = 0.6f;
    group->arrow = false;   // desplegable: sin la flecha de plegar
    PropFloat* ps[4] = { current, start, end, fps };   // el frame ACTUAL arriba de todo (es el que mas se toca)
    for (int i = 0; i < 4; i++) {
        if (!ps[i]) continue;
        ps[i]->centered = true; ps[i]->arrows = true;
        ps[i]->fineStep = 1.0f; ps[i]->thickStep = 1.0f;
        if (ps[i]->dragStep <= 0.0f) ps[i]->dragStep = 0.25f;
        group->properties.push_back(ps[i]);
    }
    group->selectIndex = 0;
    int w = 150 * GlobalScale;
    if (w > MenuScreenW - gapGS * 4) w = MenuScreenW - gapGS * 4;
    group->Resize(w, 0);
    popUpWindow->Resize(w, group->height);
    Relocate();
}
TimePopup::~TimePopup() {
    group->properties.clear();   // los PropFloat son del timeline: el grupo no los borra
    delete group;
}
void TimePopup::Relocate() {   // debajo del reloj; si no entra, arriba, y siempre dentro de la pantalla
    x = anchorX; y = anchorY;
    if (y + group->height > MenuScreenH - gapGS) y = anchorY - group->height - RowHeightGS;
    if (x + popUpWindow->width > MenuScreenW - gapGS) x = MenuScreenW - gapGS - popUpWindow->width;
    if (x < gapGS) x = gapGS; if (y < gapGS) y = gapGS;
}

void TimePopup::Render() {
    if (!group) return;
    group->Resize(popUpWindow->width, 0);
    popUpWindow->Resize(popUpWindow->width, group->height);
    Relocate();
    initView();
    w3dEngine::PushMatrix();
    group->Render();
    w3dEngine::PopMatrix();
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    group->card->RenderBorder(false);
    endView();
}

bool TimePopup::Click(int mx, int my) {
    if (!Contains(mx, my)) return false;   // afuera -> el caller lo cierra
    const int myL = my - y, lx = mx - x;
    const int boxStart = group->colLabel + 2 * borderGS;
    const int boxW = group->propertiBox->width - bordersGS;
    const int aw = RowHeightGS;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (size_t j = 0; j < group->properties.size(); j++) {
        PropertieBase* p = group->properties[j];
        const int h = p->Resize(group->width);
        if (p->Selectable() && myL >= yRow && myL < yRow + h && p->GetType() == PropertyType::Float) {
            group->selectIndex = (int)j;
            PropFloat* pf = static_cast<PropFloat*>(p);
            if (lx >= boxStart && lx < boxStart + aw)               { pf->Set(*pf->value - 1.0f); g_redraw = true; return true; }
            if (lx >= boxStart + boxW - aw && lx < boxStart + boxW) { pf->Set(*pf->value + 1.0f); g_redraw = true; return true; }
            dragField = pf; dragMoved = false; lastDragMx = mx;
            g_redraw = true;
            return true;
        }
        yRow += h;
    }
    return true;   // adentro pero fuera de un campo: consumir
}
bool TimePopup::Motion(int mx, int my) {
    (void)my;
    if (dragField && dragField->value) {
        const int dmx = mx - lastDragMx;
        // de a frames ENTEROS: se acumula hasta juntar uno
        const float step = dragField->dragStep > 0.0f ? dragField->dragStep : 0.25f;
        const int n = (int)((float)dmx * step);
        if (n != 0) { lastDragMx = mx; dragMoved = true; dragField->Set(*dragField->value + (float)n); g_redraw = true; }
    }
    return true;
}
bool TimePopup::Key(int key) {
    if (!group || group->properties.empty()) return true;
    const int n = (int)group->properties.size();
    if (group->selectIndex < 0 || group->selectIndex >= n) group->selectIndex = 0;
    PropFloat* sel = static_cast<PropFloat*>(group->properties[group->selectIndex]);
    switch (key) {
        case LayoutKey::Up:    group->selectIndex = (group->selectIndex + n - 1) % n; break;
        case LayoutKey::Down:  group->selectIndex = (group->selectIndex + 1) % n; break;
        case LayoutKey::Left:  sel->Set(*sel->value - 1.0f); break;
        case LayoutKey::Right: sel->Set(*sel->value + 1.0f); break;
        case LayoutKey::Enter:
            sel->StartEditText();
            #ifndef W3D_SYMBIAN
            if (ToolbarUsesTouch()) KeyboardNumberOpen();
            #endif
            break;
        case LayoutKey::Cancel: Close(); return true;
    }
    g_redraw = true;
    return true;
}
void TimePopup::Release() {
    if (dragField && !dragMoved && dragField->value) {
        dragField->StartEditText();            // mouse: se escribe directo en el campo
        if (ToolbarUsesTouch()) KeyboardNumberOpen();       // tactil: ademas el teclado en pantalla
    }
    dragField = NULL;
}
bool TimePopup::Dragging() { return dragField != NULL; }
void TimePopup::Close() {
    PopUpBase::Close();
    g_redraw = true;
}

void OpenTimePopup(PropFloat* current, PropFloat* start, PropFloat* end, PropFloat* fps, int ax, int ay) {
    if (gTP) { if (PopUpActive == gTP) PopUpActive = NULL; delete gTP; gTP = NULL; }
    gTP = new TimePopup(current, start, end, fps, ax, ay);
    PopUpActive = gTP;
    g_redraw = true;
}
bool TimePopupOpen() { return gTP && PopUpActive == gTP; }
// el centro (pantalla) del campo j del reloj abierto y su nombre: la MISMA cuenta de filas que Click. Lo usa el harness
// de capturas para filmar "Fin: 28" con el mouse o el dedo
bool TimePopupField(int j, int& cx, int& cy, std::string& name) {
    if (!TimePopupOpen() || !gTP->group || j < 0 || j >= (int)gTP->group->properties.size()) return false;
    GroupPropertie* g = gTP->group;
    int yRow = borderGS + RowHeightGS + gapGS;
    for (int k = 0; k < j; k++) yRow += g->properties[k]->Resize(g->width);
    const int h = g->properties[j]->Resize(g->width);
    const int boxStart = g->colLabel + 2 * borderGS, boxW = g->propertiBox->width - bordersGS;
    cx = gTP->x + boxStart + boxW / 2; cy = gTP->y + yRow + h / 2;
    name = g->properties[j]->name;
    return true;
}
